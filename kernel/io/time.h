#pragma once
#include "types.h"

void  time_init(void);
ulong time_now(void);        // Unix timestamp (seconds)
ulong time_millis(void);     // Milliseconds since boot
