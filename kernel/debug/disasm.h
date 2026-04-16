#pragma once
#include "types.h"

// Disassemble and print `n` instructions before and after `rip` to both
// the console and serial. The instruction at `rip` is marked with ">>".
void disasm_around(ulong rip, int n);
