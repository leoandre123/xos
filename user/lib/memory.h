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
  ulong n = count / 4;
  __asm__ volatile("rep movsd" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
  // handle remaining bytes (count not a multiple of 4)
  ulong rem = count % 4;
  ubyte *d = (ubyte *)dst, *s = (ubyte *)src;
  for (ulong i = 0; i < rem; i++)
    d[i] = s[i];
}

typedef struct {
  ulong size;
} malloc_hdr;

static void *malloc(ulong size) {
  ulong total = size + sizeof(malloc_hdr);
  malloc_hdr *hdr = (malloc_hdr *)sys_alloc(total);
  hdr->size = total;
  return ((void *)hdr) + sizeof(malloc_hdr);
}

static void free(void *ptr) {
  malloc_hdr *hdr = (malloc_hdr *)(ptr - sizeof(malloc_hdr));
  sys_free(hdr, hdr->size);
}

EXTERN_C_END