#include "syscall.h"
#include <stddef.h>

void *operator new(size_t n) { return sys_alloc(n); }
void *operator new[](size_t n) { return sys_alloc(n); }
void operator delete(void *) noexcept {}
void operator delete[](void *) noexcept {}
void operator delete(void *, size_t) noexcept {}
void operator delete[](void *, size_t) noexcept {}

extern "C" void __cxa_pure_virtual() {
  for (;;)
    ;
}