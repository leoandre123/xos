#include "syscall.h"

// Provided by the application
extern int main(int argc, char *argv[]);

// Kernel jumps here. Sets up nothing yet — when argc/argv passing is
// implemented, _start will receive them in rdi/rsi and forward to main.
void _start(int argc, char *argv[]) {
  int ret = main(argc, argv);
  sys_exit();
  (void)ret;
  for (;;)
    ; // unreachable
}
