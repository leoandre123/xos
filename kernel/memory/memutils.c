#include "memutils.h"
#include "types.h"

void memset64(ulong *ptr, ulong value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

void memset8(ubyte *ptr, ubyte value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

void memset16(ushort *ptr, ushort value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

void memcpy8(ubyte *dst, ubyte *src, ulong count) {
  for (ulong i = 0; i < count; i++) {
    dst[i] = src[i];
  }
}