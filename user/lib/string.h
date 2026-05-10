#pragma once
#include "cdefs.h"

EXTERN_C_BEGIN

static inline int strlen(const char *s) {
  const char *end = s;
  while (*end != '\0')
    ++end;
  return end - s;
}
static inline void strcpy(char *dst, const char *src) {
  while ((*dst++ = *src++))
    ;
}

static inline int str_starts_with(const char *s, const char *prefix) {
  while (*prefix)
    if (*s++ != *prefix++)
      return 0;
  return 1;
}

static inline int str_ends_with(const char *s, const char *suffix) {
  int slen = strlen(s), suflen = strlen(suffix);
  if (suflen > slen)
    return 0;
  s += slen - suflen;
  while (*suffix)
    if (*s++ != *suffix++)
      return 0;
  return 1;
}

EXTERN_C_END