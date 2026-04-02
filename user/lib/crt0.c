#include "syscall.h"

// Provided by the application
extern int main(void);

// Kernel jumps here. Sets up nothing yet — when argc/argv passing is
// implemented, _start will receive them in rdi/rsi and forward to main.
void _start(void) {
  int ret = main();
  sys_exit();
  (void)ret;
  for (;;)
    ; // unreachable
}
