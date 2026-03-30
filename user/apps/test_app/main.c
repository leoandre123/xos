typedef unsigned long ulong;

#define SYS_WRITE 0
#define SYS_WRITE_CONSOLE 2

static inline ulong syscall(ulong num, ulong a1, ulong a2, ulong a3) {
  ulong ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "0"(num), "D"(a1), "S"(a2), "d"(a3)
                   : "rcx", "r11", "r8", "r9", "r10", "memory", "cc");
  return ret;
}
static void sys_write_local(const char *msg) {
  __asm__ volatile("syscall"
                   :
                   : "a"(0), "D"(msg) // RAX=0 (SYS_WRITE), RDI=msg
                   : "rcx", "r11", "r8", "r9", "r10", "memory", "cc");
}

static void sys_write(const char *s) { syscall(SYS_WRITE, (ulong)s, 0, 0); }
static void sys_print(const char *s) {
  syscall(SYS_WRITE_CONSOLE, (ulong)s, 0, 0);
}

void _start() {
  sys_write_local("1");
  sys_write("1");
  sys_write_local("2");
  sys_write("2");
  sys_write_local("3");
  sys_write("3");
  sys_write_local("4");
  sys_write("4");
  sys_write_local("5\n");
  sys_write("5\n");

  sys_write_local("DONE!\n");
  for (;;) {
  }
}
