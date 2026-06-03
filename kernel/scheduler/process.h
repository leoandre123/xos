#pragma once
#include "ipc/handle.h"
#include "memory/vmm.h"
#include "process_info.h"
#include "types.h"

typedef uint pid;
typedef uint exit_code;

struct task;

typedef struct process {
  pid pid;
  pid parent_pid;
  uint next_task_index;

  char name[PROCESS_MAX_NAME_LENGTH];
  char working_directory[256];
  char executable_path[256];

  address_space *address_space;
  ulong heap_next;
  handle_entry handles[MAX_HANDLES];
  exit_code exit_code;

  struct task *main_task;
  struct task *waiting_task;
  // double linked list
  struct process *next;
  struct process *prev;

  struct process *first_child;
  struct process *next_sibling;
} process;
