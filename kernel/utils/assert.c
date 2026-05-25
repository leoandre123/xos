#include "assert.h"
#include "io/logging.h"

void _assert(const char *file, int line, const char *func, const char *expr) {
  klogf(LOG_CRITICAL, "Assert %s failed in %s (%s:%d)", expr, func, file, line);
  panic("ASSERT FAILED");
}