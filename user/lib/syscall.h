#pragma once
#include "keyboard.h"
#include "mouse.h"
#include "syscalls.h"
#include "types.h"

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
static inline void sys_write_hex(ulong value) {
  syscall(SYS_WRITE_HEX, value, 0, 0);
}
static inline void sys_exit(void) { syscall(SYS_EXIT, 0, 0, 0); }
static inline void sys_print(const char *s) {
  syscall(SYS_WRITE_CONSOLE, (ulong)s, 0, 0);
}
static inline int sys_exec(const char *path) {
  return (int)syscall(SYS_EXEC, (ulong)path, (ulong)-1, (ulong)-1);
}
static inline int sys_exec_fds(const char *path, int stdin_fd, int stdout_fd) {
  return (int)syscall(SYS_EXEC, (ulong)path, (ulong)stdin_fd, (ulong)stdout_fd);
}
static inline void sys_wait(int pid) { syscall(SYS_WAIT, (ulong)pid, 0, 0); }

// Returns: high 32 bits = ascii char, low 32 bits = keycode
static inline KeyEvent sys_read_key(void) {
  ulong r = syscall(SYS_READ_KEY, 0, 0, 0);
  return (KeyEvent){.character = r >> 32, .code = r & 0xFFFFFFFF};
}
// static inline char sys_read_char(void) { return (char)(sys_read_key() >> 32);
// }

// Pipe IPC
// Returns (write_fd << 32) | read_fd, or (ulong)-1 on error
static inline ulong sys_pipe(void) { return syscall(SYS_PIPE, 0, 0, 0); }
static inline int sys_read_fd(int fd, void *buf, ulong len) {
  return (int)syscall(SYS_READ_FD, (ulong)fd, (ulong)buf, len);
}
static inline int sys_write_fd(int fd, const void *buf, ulong len) {
  return (int)syscall(SYS_WRITE_FD, (ulong)fd, (ulong)buf, len);
}
static inline int sys_pipe_avail(int fd) {
  return (int)syscall(SYS_PIPE_AVAIL, (ulong)fd, 0, 0);
}
// Non-blocking: returns (char<<32)|keycode, or 0 if no key pending
static inline KeyEvent sys_read_key_nb(void) {
  ulong r = syscall(SYS_READ_KEY_NB, 0, 0, 0);
  return (KeyEvent){.character = r >> 32, .code = r & 0xFFFFFFFF};
}
static inline void sys_yield(void) { syscall(SYS_YIELD, 0, 0, 0); }
static inline ulong sys_time(void) { return syscall(SYS_TIME, 0, 0, 0); }
// Allocate size bytes of anonymous memory; returns user virtual address or 0
static inline void *sys_alloc(ulong size) {
  return (void *)syscall(SYS_ALLOC, size, 0, 0);
}

// Non-blocking. Returns 1 and fills *ev if a new mouse event is available, 0
// otherwise.
static inline int sys_read_mouse(mouse_event *ev) {
  ulong r = syscall(SYS_READ_MOUSE, 0, 0, 0);
  if (!r)
    return 0;
  ev->x = (int)(unsigned short)(r & 0xFFFF);
  ev->y = (int)(unsigned short)((r >> 16) & 0xFFFF);
  ev->buttons = (int)((r >> 32) & 0xFF);
  return 1;
}

