#include "process_manager.h"
#include "filesystem/elf.h"
#include "filesystem/file.h"
#include "io/logging.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "memory/vmm.h"
#include "noreturn.h"
#include "process_info.h"
#include "scheduler/process.h"
#include "scheduler/scheduler.h"
#include "scheduler/task.h"
#include "thread.h"
#include "utils/assert.h"
#include "utils/path.h"
#include "utils/string.h"

#define MAX_EXEC_ARGC 64

static pid s_next_pid = 1;

static process *s_processes = {0};
static process *s_last_process = {0};

static int handle_alloc(process *p, handle_type type, void *ptr) {
  for (int i = 0; i < MAX_HANDLES; i++) {
    if (p->handles[i].type == HANDLE_NONE) {
      p->handles[i].type = type;
      p->handles[i].ptr = ptr;
      return i;
    }
  }
  return -1;
}

static handle_entry *handle_get(process *p, int fd) {
  if (fd < 0 || fd >= MAX_HANDLES)
    return 0;
  if (p->handles[fd].type == HANDLE_NONE)
    return 0;
  return &p->handles[fd];
}

static process *create_process() {
  process *p = kmalloc(sizeof(process));
  if (!p)
    return 0;

  memset8((ubyte *)p, 0, sizeof(process));

  p->pid = s_next_pid++;
  p->next = 0;
  if (s_last_process) {
    p->prev = s_last_process;
    s_last_process->next = p;
  } else {
    p->prev = 0;
    s_processes = p;
  }

  s_last_process = p;
  return p;
}

process *find_process(pid pid) {
  for (process *p = s_processes; p; p = p->next) {
    if (p->pid == pid)
      return p;
  }
  return 0;
}

void process_manager_init() {}

static ulong push_to_user_stack(address_space *space, ulong usp, const void *src, ulong len) {
  usp -= len;
  usp &= ~7ULL;
  memcpy8((ubyte *)PHYS_TO_HHDM(vmm_virt_to_phys(space, usp)), (ubyte *)src, len);
  return usp;
}

pid process_exec(const char *path, int std_in, int std_out, int argc, const char **argv) {
  serial_printf("process exec: '%s'\n", path);
  file_handle handle = file_open(path);
  if (!handle) {
    serial_printf("SYS_EXEC: file_open failed for '%s'\n", path);
    return (pid)0;
  }

  task *current_task = scheduler_current();
  process *parent = current_task ? current_task->owner : 0;

  address_space *space = vmm_create_address_space();
  void *entry = elf_load(handle, space);
  if (!entry) {
    serial_printf("SYS_EXEC: elf_load failed for '%s'\n", path);
    vmm_destroy_address_space(space);
    return (pid)0;
  }
  task *t = task_create_user_from_space(space, entry, 0);
  file_close(handle);

  if (!t) {
    serial_printf("SYS_EXEC: elf_load failed for '%s'\n", path);
    vmm_destroy_address_space(space);
    return (pid)0;
  }

  // Push argv strings and pointer array onto user stack
  if (argc <= 0 || !argv)
    argc = 0;
  if (argc > MAX_EXEC_ARGC)
    argc = MAX_EXEC_ARGC;

  ulong usp = (ulong)t->user_rsp;
  ulong argv_user[MAX_EXEC_ARGC + 1];

  for (int i = argc - 1; i >= 0; i--) {
    ulong len = 0;
    while (argv[i][len])
      len++;
    len++;
    usp = push_to_user_stack(space, usp, argv[i], len);
    argv_user[i] = usp;
  }
  argv_user[argc] = 0;

  usp -= (argc + 1) * 8;
  usp &= ~15ULL;
  memcpy8((ubyte *)PHYS_TO_HHDM(vmm_virt_to_phys(space, usp)),
          (ubyte *)argv_user, (argc + 1) * 8);

  t->start_argc = (ulong)argc;
  t->start_argv = usp;             // argv array lives here (16-byte aligned)
  t->user_rsp = (void *)(usp - 8); // RSP 8 below → 8-mod-16 (matches "after call" ABI)

  klogf(LOG_TRACE, "Process main task stack at: %x", t->stack_base);

  process *p = create_process();
  p->address_space = space;
  p->parent_pid = parent ? parent->pid : 0;
  p->main_task = t;
  p->heap_next = USER_HEAP_BASE;
  t->owner = p;
  p->next_task_index = 1;

  strcpy(p->working_directory, path);
  path_dirname(path, p->working_directory, sizeof(p->working_directory));
  strcpy(p->name, path_filename(path));

  if (parent) {
    p->next_sibling = parent->first_child;
    parent->first_child = p;
  }

  if (std_in != -1) {
    handle_entry *h = handle_get(parent, std_in);
    if (h)
      p->handles[0] = *h;
  }
  if (std_out != -1) {
    handle_entry *h = handle_get(parent, std_out);
    if (h)
      p->handles[1] = *h;
  }
  scheduler_add(t);

  return p->pid;
}
exit_code process_wait(pid pid) {
  process *p = find_process(pid);
  if (!p)
    return (exit_code)-1;
  if (p->waiting_task)
    return (exit_code)-1;

  task *t = scheduler_current();

  p->waiting_task = t;
  task_set_blocked(t);
  schedule();

  exit_code code = p->exit_code;
  process_delete(p);
  return code;
}

int process_list(process_info *buf, int count) {
  int i = 0;
  for (process *p = s_processes; p && i < count; p = p->next, i++) {
    buf[i].pid = p->pid;
    strcpy(buf[i].name, p->name);
    buf[i].state = (proc_state)p->main_task->state;
  }

  return i;
}

thread_handle process_spawn_thread(void(*entry), ulong arg) {
  task *current_task = scheduler_current();
  process *p = current_task->owner;
  ASSERT(p);

  task *t = task_create_user_from_space(p->address_space, entry, p->next_task_index++);
  if (!t) {
    klogf(LOG_WARNING, "Failed to spawn new task for process %s", p->name);
    return (thread_handle)0;
  }
  t->start_argc = arg;
  t->owner = p;
  scheduler_add(t);
  return (thread_handle)t;
}

ulong process_join_thread(thread_handle h) {
  task *t = (task *)h;
  task *current = scheduler_current();
  klogf(LOG_TRACE, "Waiting for thread join of thread %d (State: %d) into thread %d", t->id, t->state, current->id);

  if (t->state == TASK_DEAD)
    return t->ret_val;

  if (t->joining_task)
    return -1;

  t->joining_task = current;
  task_set_blocked(current);
  schedule();
  klogf(LOG_TRACE, "Successfully joined thread %d", t->id);
  return t->ret_val;
}

NORETURN process_exit_thread(ulong ret_val) {
  task *t = scheduler_current();
  t->ret_val = ret_val;

  klogf(LOG_TRACE, "Thread %d exited", t->id);
  task_exit();
}

void process_kill_thread(thread_handle h) {
  task *t = (task *)h;
  task_kill(t);
}
NORETURN process_exit(exit_code code) {
  task *current_task = scheduler_current();
  process *p = current_task->owner;
  process_kill(p);
  schedule();
  __builtin_unreachable();
}

// Called when a process main thread exits or mannually to kill the process
void process_kill(process *p) {
  task *t = p->main_task;
  while (t) {
    task *next = t->next_in_process;
    if (t->state != TASK_DEAD) {
      task_kill(t);
    }
    t = next;
  }

  if (p->waiting_task) {
    task_set_ready(p->waiting_task);
  }
}

void process_delete(process *p) {
  task *t = p->main_task;
  while (t) {
    task *next = t->next_in_process;
    task_delete(t);
    t = next;
  }
  kfree(p);
}