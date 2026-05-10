#pragma once
#include "cdefs.h"
#include "syscall.h"
#include "syscalls.h"
#include "types.h"

EXTERN_C_BEGIN

typedef ulong file_handle;

typedef struct {
  char name[247];
  bool is_dir;
  ulong file_size;
} dirent;

static inline file_handle file_open(const char *path) {
  return syscall(SYS_FILE_OPEN, (ulong)path, 0, 0);
}
static inline void file_close(file_handle handle) {
  syscall(SYS_FILE_CLOSE, (ulong)handle, 0, 0);
}
static inline int file_readdir(const char *path, dirent *out, int max) {
  return syscall(SYS_FILE_READDIR, (ulong)path, (ulong)out, max);
}
static inline uint file_read(file_handle handle, void *buf, uint count) {
  return syscall(SYS_FILE_READ, (ulong)handle, (ulong)buf, count);
}

static inline uint file_size(file_handle handle) {
  return syscall(SYS_FILE_SIZE, (ulong)handle, 0, 0);
}

EXTERN_C_END