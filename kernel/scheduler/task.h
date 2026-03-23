#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum { TASK_READY, TASK_RUNNING, TASK_BLOCKED, TASK_DEAD } task_state_t;

typedef struct cpu_context {
  uint64_t r15, r14, r13, r12;
  uint64_t r11, r10, r9, r8;
  uint64_t rsi, rdi, rbp;
  uint64_t rdx, rcx, rbx, rax;

  uint64_t rip;
  uint64_t rflags;
  uint64_t rsp;
} cpu_context_t;

typedef struct task {
  int pid;
  task_state_t state;

  cpu_context_t context;

  void *stack;
  size_t stack_size;

  struct task *next;
} task_t;