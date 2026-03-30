#pragma once
#include "memory/vmm.h"
#include "task.h"

#define USER_STACK_TOP   0x00007FFFFFFFF000 // top of user virtual address space
#define USER_STACK_PAGES 4

void scheduler_init();
void scheduler_add(task *task);
void scheduler_run();
void schedule();
task *scheduler_find(int pid);
void task_exit(void);
task *task_create_kernel(void (*entry)(void *), void *args, const char *name);
task *task_create_user(void (*entry)(void *), void *args, const char *name);
task *task_create_user_from_space(address_space *space, void *entry);