#pragma once
#include "types.h"

extern ulong g_syscall_kernel_rsp;

void syscall_init(void);
ulong syscall_dispatch(ulong num, ulong arg1, ulong arg2, ulong arg3);
