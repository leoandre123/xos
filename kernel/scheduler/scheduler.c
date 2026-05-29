#include "scheduler.h"
#include "cpu/gdt.h"
#include "cpu/syscall.h"
#include "io/logging.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "noreturn.h"
#include "panic.h"
#include "perf/perf.h"
#include "scheduler/process.h"
#include "scheduler/process_manager.h"
#include "task.h"
#include "types.h"

extern void context_switch(ulong *old_rsp, ulong new_rsp);

int g_scheduler_running = 0;

static task *g_current = 0;
static task *g_task_list = 0;
static task *g_idle = 0;
static uint s_next_id = 1;

static NORETURN task_bootstrap() {
  __asm__ volatile("sti");
  g_current->entry(g_current->args);
  task_exit();
  panic("task_bootstrap returned");
}

static NORETURN user_task_bootstrap() {
  serial_printf("user_task_bootstrap: task='%s' entry=%x\n", g_current->owner->name, (ulong)g_current->entry);
  gdt_set_kernel_stack((ulong)g_current->stack_base + g_current->stack_size);
  jump_to_userspace((ulong)g_current->entry, (ulong)g_current->user_rsp,
                    g_current->start_argc, g_current->start_argv);
  panic("jump_to_userspace returned");
}

static task *scheduler_pick_next(void) {
  if (!g_current) {
    return g_task_list;
  }

  if (g_current == g_idle || g_current->state == TASK_DEAD) {
    // g_idle is not in the task list; a DEAD task has been removed from it.
    // Either way, scan from the list head since we can't use g_current as
    // a loop terminator.
    if (!g_task_list)
      return g_idle;
    task *t = g_task_list;
    do {
      if (t->state == TASK_READY)
        return t;
      t = t->next;
    } while (t != g_task_list);
    return g_idle;
  }

  task *t = g_current->next;

  // Loop over the next tasks after the current one until a ready one is found or we are back where we started
  while (t != g_current) {
    if (t->state == TASK_READY) {
      return t;
    }
    t = t->next;
  }

  // If we get here, no other task is ready. Thus we check if the current one is, otherwise idle
  if (g_current->state == TASK_READY || g_current->state == TASK_RUNNING) {
    return g_current;
  }

  return g_idle;
}
static void idle() {
  for (;;) {
    PERF_SCOPE("idle");
    asm volatile("hlt");
  }
}
static task *task_create(void (*entry)(void *), void *args) {
  task *t = kmalloc(sizeof(task));
  if (!t)
    return 0;
  memset8((ubyte *)t, 0, sizeof(task));

  void *stack = kmalloc(4096);
  if (!stack) {
    kfree(t);
    return 0;
  }
  memset8(stack, 0, 4096);

  t->id = s_next_id++;
  t->stack_base = stack;
  t->stack_size = 4096;
  t->next = 0;
  t->state = TASK_READY;
  t->entry = entry;
  t->args = args;
  t->task_index = 0;

  ulong *sp = stack + 4096;
  sp = (void *)((ulong)sp & ~0xFULL);

  *--sp = (ulong)task_bootstrap;
  *--sp = 0; // rbx
  *--sp = 0; // rbp
  *--sp = 0; // r12
  *--sp = 0; // r13
  *--sp = 0; // r14
  *--sp = 0; // r15

  t->kernel_rsp = sp;

  return t;
}

void schedule() {
  if (!g_scheduler_running)
    return;

  PERF_BEGIN("schedule");

  task *next = scheduler_pick_next();
  task *prev = g_current;
  if (!next || next == prev) {
    PERF_END();
    return;
  }

  if (prev && prev->state == TASK_RUNNING)
    prev->state = TASK_READY;

  g_current = next;
  g_current->state = TASK_RUNNING;

  PERF_MODE_SWITCH(prev, next);

  __asm__ volatile("cli");

  if (next->owner && next->owner->address_space) {
    vmm_switch_address_space(next->owner->address_space);
    ulong kstack_top = (ulong)next->stack_base + next->stack_size;
    gdt_set_kernel_stack(kstack_top);
    g_syscall_kernel_rsp = kstack_top;
  } else {
    vmm_switch_address_space(&g_kernel_address_space);
  }

  PERF_END();
  if (prev) {
    context_switch((ulong *)&prev->kernel_rsp, (ulong)next->kernel_rsp);
  } else {
    ulong dummy = 0;
    serial_write_line("First context_switch");
    context_switch(&dummy, (ulong)next->kernel_rsp);
  }

  PERF_MODE_RESUME(g_current);

  __asm__ volatile("sti");
}
task *scheduler_find(uint id) {
  if (!g_task_list)
    return 0;
  task *t = g_task_list;
  do {
    if (t->id == id)
      return t;
    t = t->next;
  } while (t != g_task_list);
  return 0;
}

void scheduler_init() {
  g_idle = task_create_kernel(idle, 0);
}
void scheduler_add(task *task) {
  if (!g_task_list) {
    g_task_list = task;
    task->next = task;
    task->prev = task;
    return;
  }

  /*
   * g_task_list(a)<->b<->c
   *    ^-----------------^
   *
   * scheduler_add(d):
   *
   * g_task_list(a)<->b<->c<->d
   *    ^---------------------^
   */

  task->prev = g_task_list->prev;
  task->next = g_task_list;

  g_task_list->prev->next = task;
  g_task_list->prev = task;
}

void scheduler_remove(task *t) {
  t->prev->next = t->next;
  t->next->prev = t->prev;
}

NORETURN scheduler_run() {
  g_scheduler_running = 1;
  schedule();
  panic("schedule_run has returned");
}

task *scheduler_current(void) { return g_current; }

NORETURN task_exit(void) {
  task_kill(g_current);

  schedule();
  panic("task_exit returned");
}

void task_kill(task *t) {
  t->state = TASK_DEAD;
  scheduler_remove(t);
  klogf(LOG_TRACE, "task_kill t-ind: %d (%x)", t->task_index, t->joining_task);
  if (t->task_index == 0) {
    process *p = t->owner;
    process_kill(p);
  } else if (t->joining_task) {
    klogf(LOG_TRACE, "Joining task %d into %d", t->id, t->joining_task->id);
    task_set_ready(t->joining_task);
  }
}

void task_delete(task *t) {
  kfree(t);
}

task *task_create_kernel(void (*entry)(void *), void *args) {
  return task_create(entry, args);
}

task *task_create_user_from_space(address_space *space, void *entry, uint task_index) {
  task *t = task_create(entry, 0);
  t->task_index = task_index;

  klogf(LOG_TRACE, "task create with index: %d", task_index);

  // allocate and map user stack
  ulong phys_stack = pmm_alloc_pages(USER_STACK_PAGES);
  ulong user_stack_top = USER_STACK_TOP - (USER_STACK_PAGES * PAGE_SIZE * task_index);
  ulong user_stack_base = user_stack_top - (USER_STACK_PAGES * PAGE_SIZE);
  vmm_map_pages(space, user_stack_base, phys_stack, USER_STACK_PAGES,
                PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);

  t->user_rsp = (void *)user_stack_top - 8; // where rsp points when entering ring 3
  ulong *sp = t->kernel_rsp;
  sp[6] = (ulong)user_task_bootstrap; // overwrite the return address

  return t;
}

typedef struct {
  task *t;
  ulong wake_tick;
} sleep_queue_entry;

#define SLEEP_QUEUE_MAX 256
static sleep_queue_entry g_sleep_queue[SLEEP_QUEUE_MAX];
static int g_sleep_queue_len = 0;

bool sleep_queue_enqueue(task *t, ulong wake_tick) {
  if (g_sleep_queue_len >= SLEEP_QUEUE_MAX)
    return false;

  int i = g_sleep_queue_len++;
  while (i > 0 && g_sleep_queue[i - 1].wake_tick > wake_tick) {
    g_sleep_queue[i] = g_sleep_queue[i - 1];
    i--;
  }
  g_sleep_queue[i] = (sleep_queue_entry){.t = t, .wake_tick = wake_tick};
  return true;
}

void sleep_queue_wake(ulong current_tick) {
  PERF_SCOPE("sleep_queue_wake");
  int i = 0;
  while (i < g_sleep_queue_len && g_sleep_queue[i].wake_tick <= current_tick) {
    task_set_ready(g_sleep_queue[i].t);
    i++;
  }
  if (i > 0) {
    g_sleep_queue_len -= i;
    for (int j = 0; j < g_sleep_queue_len; j++)
      g_sleep_queue[j] = g_sleep_queue[i + j];
  }
}
