#pragma once

static inline int strlen(const char *s) {
  const char *end = s;
  while (*end != '\0')
    ++end;
  return end - s;
}