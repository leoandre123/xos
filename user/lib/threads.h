#pragma once
#include "cdefs.h"
#include "noreturn.h"
#include "thread.h"
#include "types.h"

EXTERN_C_BEGIN

thread_handle thread_spawn(void *entry, ulong arg);
ulong thread_join(thread_handle h);
void thread_join_all();
NORETURN thread_exit(ulong ret_val);
void thread_kill(thread_handle h);

EXTERN_C_END