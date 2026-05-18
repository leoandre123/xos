#pragma once
#include "types.h"

struct process;

typedef enum : ubyte { TASK_READY,
                       TASK_RUNNING,
                       TASK_BLOCKED,
                       TASK_DEAD } task_state;

typedef struct task {
  uint id;
  task_state state;

  struct process *owner;

  void (*entry)(void *);
  void *args;
  void *kernel_rsp;
  void *user_rsp;
  void *stack_base;
  ulong stack_size;
  struct task *next;
} task;