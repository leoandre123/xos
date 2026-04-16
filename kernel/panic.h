#pragma once
#include "types.h"

typedef struct {
  ulong rax, rbx, rcx, rdx;
  ulong rsi, rdi, rbp, rsp;
  ulong r8,  r9,  r10, r11;
  ulong r12, r13, r14, r15;
  ulong rip, rflags;
} panic_regs;

void panic_impl(const char *msg, const panic_regs *regs);

// Capture registers before any C code runs, then call panic_impl.
// leaq 0(%%rip) captures the address of the next instruction inside the
// asm sequence, which is within a few bytes of the panic() call site.
#define panic(msg) do {                                          \
  panic_regs _r;                                                 \
  __asm__ volatile(                                              \
    /* save rax first so we can reuse it as scratch for RIP */   \
    "mov %%rax,%0\n\t"                                           \
    "mov %%rbx,%1\n\t"  "mov %%rcx,%2\n\t"                      \
    "mov %%rdx,%3\n\t"  "mov %%rsi,%4\n\t"                      \
    "mov %%rdi,%5\n\t"  "mov %%rbp,%6\n\t"                      \
    "mov %%rsp,%7\n\t"  "mov %%r8,%8\n\t"                       \
    "mov %%r9,%9\n\t"   "mov %%r10,%10\n\t"                     \
    "mov %%r11,%11\n\t" "mov %%r12,%12\n\t"                      \
    "mov %%r13,%13\n\t" "mov %%r14,%14\n\t"                     \
    "mov %%r15,%15\n\t"                                          \
    /* rax is already saved, safe to clobber as RIP scratch */   \
    "leaq 0(%%rip),%%rax\n\t"                                    \
    "mov %%rax,%16\n\t"                                          \
    "pushfq\n\t" "popq %17\n\t"                                  \
    : "=m"(_r.rax), "=m"(_r.rbx),                               \
      "=m"(_r.rcx), "=m"(_r.rdx),                               \
      "=m"(_r.rsi), "=m"(_r.rdi),                               \
      "=m"(_r.rbp), "=m"(_r.rsp),                               \
      "=m"(_r.r8),  "=m"(_r.r9),                                \
      "=m"(_r.r10), "=m"(_r.r11),                               \
      "=m"(_r.r12), "=m"(_r.r13),                               \
      "=m"(_r.r14), "=m"(_r.r15),                               \
      "=m"(_r.rip), "=m"(_r.rflags)                             \
    :: "rax", "memory");                                          \
  panic_impl(msg, &_r);                                          \
} while (0)

void panic_assert(const char *file, int line, const char *expr);
