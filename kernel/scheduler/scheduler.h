#pragma once
#include "memory/vmm.h"
#include "noreturn.h"
#include "scheduler/process.h"
#include "task.h"

#define USER_STACK_TOP   0x0000800000000000ULL // top of user virtual address space
#define USER_STACK_PAGES 4
#define USER_HEAP_BASE   0x0000500000000000ULL // base of user heap (grows up)

extern int g_scheduler_running;
extern task *g_current_task;

void scheduler_init();
void scheduler_add(task *task);
static inline task *scheduler_current(void) { return g_current_task; }
static inline process *process_current(void) { return g_current_task->owner; }
NORETURN scheduler_run();
void schedule();
task *scheduler_find(uint id);
NORETURN task_exit(void);

void task_kill(task *t);   // Kills task and frees all its resources, the task struct itself remains
void task_delete(task *t); // Deletes the task struct

task *task_create_kernel(void (*entry)(void *), void *args);
task *task_create_user_from_space(address_space *space, void *entry, uint task_index);

bool sleep_queue_enqueue(task *t, ulong wake_tick);
void sleep_queue_wake(ulong current_tick);

// TODO: Make these functions add/remove the task from the scheduler queue
// Maybe rename to scheduler_ready_task and scheduler_block_task
static inline void task_set_ready(task *t) {
  if (t->state == TASK_BLOCKED) {
    t->state = TASK_READY;
  }
}
static inline void task_set_blocked(task *t) {
  if (t->state != TASK_DEAD) {
    t->state = TASK_BLOCKED;
  }
}