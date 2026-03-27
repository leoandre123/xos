#pragma once

#include "memory/vmm.h"
#include "types.h"
typedef enum { TASK_READY,
               TASK_RUNNING,
               TASK_BLOCKED,
               TASK_DEAD } task_state;

typedef struct task {
  int pid;
  const char *name;
  task_state state;
  address_space *address_space;
  void (*entry)(void *);
  void *args;
  void *rsp;
  void *user_rsp;
  void *stack_base;
  ulong stack_size;

  struct task *next;
} task;