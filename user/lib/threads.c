#include "threads.h"
#include "memory.h"
#include "syscall.h"
#include "syscalls.h"
#include "thread.h"

typedef struct {
  ulong (*entry)();
  ulong arg;
} thread_start_args;

static void thread_trampoline(ulong arg) {
  thread_start_args *a = (thread_start_args *)arg;
  ulong (*entry)(ulong) = a->entry;
  ulong real_arg = a->arg;
  free(a);
  ulong ret_val = entry(real_arg);
  syscall(SYS_THREAD_EXIT, ret_val, 0, 0);
}

thread_handle thread_spawn(void *entry, ulong arg) {
  thread_start_args *args = malloc(sizeof(thread_start_args));
  args->entry = entry;
  args->arg = arg;
  return (thread_handle)syscall(SYS_THREAD, (ulong)thread_trampoline,
                                (ulong)args, 0);
}

ulong thread_join(thread_handle h) {
  return syscall(SYS_THREAD_JOIN, (ulong)h, 0, 0);
}
void thread_join_all() { syscall(SYS_THREAD_JOIN, 0, 0, 0); }
NORETURN thread_exit(ulong ret_val) { syscall(SYS_THREAD_EXIT, ret_val, 0, 0); }
void thread_kill(thread_handle h) { syscall(SYS_THREAD_KILL, (ulong)h, 0, 0); }