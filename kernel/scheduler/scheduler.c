#include "scheduler.h"
#include "cpu/gdt.h"
#include "cpu/syscall.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "panic.h"
#include "scheduler/process.h"
#include "task.h"
#include "types.h"

extern void context_switch(ulong *old_rsp, ulong new_rsp);

int g_scheduler_running = 0;

static task *g_current = 0;
static task *g_task_list = 0;
static task *g_idle = 0;
static uint s_next_id = 1;

task *scheduler_current(void) { return g_current; }

__attribute__((noreturn)) void task_exit(void) {
  g_current->state = TASK_DEAD;

  schedule();
  panic("task_exit returned");
}

__attribute__((noreturn)) void task_bootstrap() {
  __asm__ volatile("sti");
  g_current->entry(g_current->args);
  // If task function returns, kill task
  task_exit();

  for (;;) {
    __asm__ volatile("cli; hlt");
  }
}

__attribute__((noreturn)) static void user_task_bootstrap() {
  serial_printf("user_task_bootstrap: task='%s' entry=%x\n", g_current->owner->name, (ulong)g_current->entry);
  gdt_set_kernel_stack((ulong)g_current->stack_base + g_current->stack_size);
  jump_to_userspace((ulong)g_current->entry, (ulong)g_current->user_rsp);
}

static task *scheduler_pick_next(void) {
  if (!g_current) {
    return g_task_list;
  }

  task *t = g_current->next;

  while (t != g_current) {
    if (t->state == TASK_READY) {
      return t;
    }
    t = t->next;
  }

  if (g_current->state == TASK_READY || g_current->state == TASK_RUNNING) {
    return g_current;
  }

  return g_idle;
}
static void idle() {
  for (;;)
    asm volatile("hlt");
}
static inline task *task_create(void (*entry)(void *), void *args) {
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
  task *next = scheduler_pick_next();
  task *prev = g_current;
  if (!next || next == prev)
    return;

  if (prev && prev->state == TASK_RUNNING)
    prev->state = TASK_READY;

  g_current = next;
  g_current->state = TASK_RUNNING;

  __asm__ volatile("cli");

  if (next->owner && next->owner->address_space) {
    vmm_switch_address_space(next->owner->address_space);
    ulong kstack_top = (ulong)next->stack_base + next->stack_size;
    gdt_set_kernel_stack(kstack_top);
    g_syscall_kernel_rsp = kstack_top;
  } else {
    vmm_switch_address_space(&g_kernel_address_space);
  }

  if (prev) {
    context_switch((ulong *)&prev->kernel_rsp, (ulong)next->kernel_rsp);
  } else {
    ulong dummy = 0;
    serial_write_line("First context_switch");
    context_switch(&dummy, (ulong)next->kernel_rsp);
  }

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
    return;
  }

  task->next = g_task_list->next;
  g_task_list->next = task;
}
__attribute__((noreturn)) void scheduler_run() {
  g_scheduler_running = 1;
  schedule();
  panic("schedule_run has returned");
}

task *task_create_kernel(void (*entry)(void *), void *args) {
  return task_create(entry, args);
}

task *task_create_user_from_space(address_space *space, void *entry, uint task_index) {
  task *t = task_create(entry, 0);

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