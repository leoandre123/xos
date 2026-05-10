#include "syscall.h"

extern int main(int argc, char *argv[]);

// Defined by the linker script; bracket the .init_array section.
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

void _start(int argc, char *argv[]) {
  for (void (**fn)(void) = __init_array_start; fn < __init_array_end; fn++)
    (*fn)();

  int ret = main(argc, argv);
  sys_exit();
  (void)ret;
  for (;;)
    ;
}
