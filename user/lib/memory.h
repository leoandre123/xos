#pragma once
#include "cdefs.h"

#include "syscall.h"
#include "types.h"

EXTERN_C_BEGIN

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
static void inline memcpy(void *dst, const void *src, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ((ubyte *)dst)[i] = ((ubyte *)src)[i];
  }
}

static void *malloc(ulong size) { return sys_alloc(size); }

static void free(void *ptr) {
  // TODO: implement
}

EXTERN_C_END