#pragma once
#include "types.h"

// Number used by user programs to write a string to serial
#define SYS_WRITE         0
#define SYS_EXIT          1
#define SYS_WRITE_CONSOLE 2
#define SYS_READ_KEY      3   // blocks until key; returns (char<<32)|keycode
#define SYS_EXEC          4   // arg1=path; returns pid or -1
#define SYS_WAIT          5   // arg1=pid; blocks until that pid exits

extern ulong g_syscall_kernel_rsp;

void syscall_init(void);
ulong syscall_dispatch(ulong num, ulong arg1, ulong arg2, ulong arg3);
