#pragma once
#include "types.h"

void time_init(void);
ulong time_unix(void); // Unix timestamp (seconds)
ulong time_unix_millis(void);
ulong time_sys_millis(void); // Milliseconds since boot
