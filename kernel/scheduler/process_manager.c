#include "process_manager.h"
#include "filesystem/elf.h"
#include "filesystem/file.h"
#include "io/logging.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/vmm.h"
#include "scheduler/process.h"
#include "scheduler/scheduler.h"
#include "utils/path.h"
#include "utils/string.h"

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

static process *find_process(pid pid) {
  for (process *p = s_processes; p; p = p->next) {
    if (p->pid == pid)
      return p;
  }
  return 0;
}

void process_manager_init() {}

pid process_exec(const char *path, int std_in, int std_out) {
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

  klogf(LOG_TRACE, "Process main task stack at: %x", t->stack_base);

  process *p = create_process();
  p->address_space = space;
  p->parent_pid = parent->pid;
  p->main_task = t;
  p->heap_next = USER_HEAP_BASE;
  t->owner = p;
  p->next_task_index = 1;

  strcpy(p->working_directory, path);
  path_dirname(path, p->working_directory, sizeof(p->working_directory));
  strcpy(p->name, path_filename(path));

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

  __asm__ volatile("sti");
  while (p->main_task->state != TASK_DEAD)
    schedule();
  __asm__ volatile("cli");
  return p->exit_code;
}

int process_list(process_info *buf, int count) {
  int i = 0;
  for (process *p = s_processes; p && i < count; p = p->next, i++) {
    buf[i].pid = p->pid;
    strcpy(buf[i].name, p->name);
  }

  return i;
}