#include "../../lib/syscall.h"

static unsigned int slen(const char *s) {
  unsigned int n = 0;
  while (s[n]) n++;
  return n;
}

static void print(const char *s) {
  sys_write_fd(1, s, slen(s));
}

void _start() {
  print("Hello from test_app!\n");
  print("12345\n");
  sys_exit();
}
