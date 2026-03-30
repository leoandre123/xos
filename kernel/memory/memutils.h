#pragma once
#include "types.h"

void memset64(ulong *ptr, ulong value, ulong count);
void memset8(ubyte *ptr, ubyte value, ulong count);

void memcpy8(ubyte *dst, ubyte *src, ulong count);