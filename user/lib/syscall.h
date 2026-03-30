#pragma once

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char ubyte;

#define SYS_WRITE 0
#define SYS_EXIT 1
#define SYS_WRITE_CONSOLE 2
#define SYS_READ_KEY 3
#define SYS_EXEC 4
#define SYS_WAIT 5

static inline ulong syscall(ulong num, ulong a1, ulong a2, ulong a3) {
  ulong ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(num), "D"(a1), "S"(a2), "d"(a3)
                   : "rcx", "r11", "r8", "r9", "r10", "memory", "cc");
  return ret;
}

static inline void sys_write(const char *s) {
  syscall(SYS_WRITE, (ulong)s, 0, 0);
}
static inline void sys_exit(void) { syscall(SYS_EXIT, 0, 0, 0); }
static inline void sys_print(const char *s) {
  syscall(SYS_WRITE_CONSOLE, (ulong)s, 0, 0);
}
static inline int sys_exec(const char *path) {
  return (int)syscall(SYS_EXEC, (ulong)path, 0, 0);
}
static inline void sys_wait(int pid) { syscall(SYS_WAIT, (ulong)pid, 0, 0); }

// Returns: high 32 bits = ascii char, low 32 bits = keycode
static inline ulong sys_read_key(void) {
  return syscall(SYS_READ_KEY, 0, 0, 0);
}

static inline char sys_read_char(void) { return (char)(sys_read_key() >> 32); }
