#pragma once

#include "types.h"
static void inline memset64(ulong *ptr, ulong value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static void inline memset8(ubyte *ptr, ubyte value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static void inline memset16(ushort *ptr, ushort value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static void inline memcpy8(ubyte *dst, ubyte *src, ulong count) {
  for (ulong i = 0; i < count; i++) {
    dst[i] = src[i];
  }
}