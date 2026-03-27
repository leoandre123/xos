#pragma once
#include "task.h"

#define USER_STACK_TOP   0x00007FFFFFFFFFFF // top of user virtual address space
#define USER_STACK_PAGES 4

void scheduler_init();
void scheduler_add(task *task);
void scheduler_run();
void schedule();
task *task_create_kernel(void (*entry)(void *), void *args, const char *name);
task *task_create_user(void (*entry)(void *), void *args, const char *name);