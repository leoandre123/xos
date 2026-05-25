#pragma once
#include "types.h"

struct process;

typedef enum : ubyte { TASK_READY,
                       TASK_RUNNING,
                       TASK_BLOCKED,
                       TASK_DEAD } task_state;

typedef struct task {
  uint id;
  uint task_index;
  task_state state;
  struct process *owner;
  struct task *joining_task;

  void (*entry)(void *);
  void *args;
  void *kernel_rsp;
  void *user_rsp;
  void *stack_base;
  ulong stack_size;
  ulong start_argc;
  ulong start_argv;

  ulong ret_val;

  //
  struct task *next_in_process;
  struct task *prev_in_process;

  // Scheduler linked list
  struct task *next;
  struct task *prev;
} task;