#pragma once

#include "ipc/handle.h"
#include "memory/vmm.h"
#include "types.h"
typedef enum { TASK_READY,
               TASK_RUNNING,
               TASK_BLOCKED,
               TASK_DEAD } task_state;

typedef struct task {
  int pid;
  task_state state;
  char name[56];
  address_space *address_space;
  void (*entry)(void *);
  void *args;
  void *rsp;
  void *user_rsp;
  void *stack_base;
  ulong stack_size;
  handle_entry handles[MAX_HANDLES];
  char working_directory[256];
  struct task *next;
} task;