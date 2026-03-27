#pragma once
#include "panic.h"

#define STRINGIFY2(x) #x
#define STRINGIFY(x)  STRINGIFY2(x)

#define ASSERT(expr)                                                          \
  do {                                                                        \
    if (!(expr)) {                                                            \
      panic("ASSERT failed: " #expr " at " __FILE__ ":" STRINGIFY(__LINE__)); \
    }                                                                         \
  } while (0)