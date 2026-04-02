#pragma once
#include "types.h"

// Number used by user programs to write a string to serial
#define SYS_WRITE         0
#define SYS_EXIT          1
#define SYS_WRITE_CONSOLE 2
#define SYS_READ_KEY      3  // blocks until key; returns (char<<32)|keycode
#define SYS_EXEC          4  // arg1=path, arg2=stdin_fd, arg3=stdout_fd; returns pid or -1
#define SYS_WAIT          5  // arg1=pid; blocks until that pid exits
#define SYS_PIPE          6  // returns (write_fd<<32)|read_fd
#define SYS_READ_FD       7  // arg1=fd, arg2=buf, arg3=len; returns bytes read
#define SYS_WRITE_FD      8  // arg1=fd, arg2=buf, arg3=len; returns bytes written
#define SYS_MAP_FB        9  // arg1=fb_info*; maps framebuffer into task, fills struct
#define SYS_PIPE_AVAIL    10 // arg1=fd; returns bytes available without blocking
#define SYS_READ_KEY_NB   11 // non-blocking; returns (char<<32)|code or 0 if no key
#define SYS_YIELD         12 // yield CPU to scheduler
#define SYS_WRITE_HEX     20

extern ulong g_syscall_kernel_rsp;

void syscall_init(void);
ulong syscall_dispatch(ulong num, ulong arg1, ulong arg2, ulong arg3);
