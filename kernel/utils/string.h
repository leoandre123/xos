#pragma once

static inline void strcpy(char *dst, const char *src) {
  while ((*dst++ = *src++))
    ;
}