#pragma once
#include "panic.h"

#define STRINGIFY2(x) #x
#define STRINGIFY(x)  STRINGIFY2(x)

// #define ASSERT(expr)                                                          \
//   do {                                                                        \
//     if (!(expr)) {                                                            \
//       panic("ASSERT failed: " #expr " at " __FILE__ ":" STRINGIFY(__LINE__)); \
//     }                                                                         \
//   } while (0)

#define ASSERT(expr) ((expr) ? (void)0 : _assert(__FILE__, __LINE__, __func__, #expr));

void _assert(const char *file, int line, const char *func, const char *expr);