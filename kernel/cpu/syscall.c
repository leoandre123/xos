#include "syscall.h"
#include "filesystem/elf.h"
#include "filesystem/fat32.h"
#include "gdt.h"
#include "graphics/console.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "io/serial.h"
#include "scheduler/scheduler.h"
#include "scheduler/task.h"
#include "types.h"

#define MSR_EFER         0xC0000080
#define MSR_STAR         0xC0000081
#define MSR_LSTAR        0xC0000082
#define MSR_SYSCALL_MASK 0xC0000084

// Updated by the scheduler whenever a user task becomes active.
// The syscall handler switches RSP to this value on entry.
ulong g_syscall_kernel_rsp = 0;

extern void syscall_entry(void);

static ulong rdmsr(uint msr) {
  uint lo, hi;
  __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
  return ((ulong)hi << 32) | lo;
}

static void wrmsr(uint msr, ulong val) {
  __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint)val), "d"((uint)(val >> 32)));
}

void syscall_init(void) {
  // Enable SCE (syscall enable) in EFER
  wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 1);

  // STAR layout:
  //   bits[47:32] = kernel CS (0x08) → syscall sets CS=0x08, SS=0x10
  //   bits[63:48] = user base (0x18) → sysretq sets CS=0x18+16=0x2B, SS=0x18+8=0x23
  ulong star = ((ulong)0x18 << 48) | ((ulong)KERNEL_CODE_SEL << 32);
  wrmsr(MSR_STAR, star);

  // Point the CPU at our handler
  wrmsr(MSR_LSTAR, (ulong)syscall_entry);

  // Clear IF on syscall entry so we don't get preempted mid-handler
  wrmsr(MSR_SYSCALL_MASK, 0x200);
}

ulong syscall_dispatch(ulong num, ulong arg1, ulong arg2, ulong arg3) {
  (void)arg2;
  (void)arg3;

  // serial_write("SYSCALL: ");
  // serial_write_hex8(num);
  // serial_write_char('\n');

  switch (num) {
  case SYS_WRITE:
    serial_write((const char *)arg1);
    return 0;

  case SYS_EXIT:
    task_exit();
    return 0;

  case SYS_WRITE_CONSOLE:
    console_write((const char *)arg1);
    return 0;

  case SYS_READ_KEY: {
    KeyEvent ev;
    // serial_write_line("SYS_READ_KEY: enabling interrupts and halting");
    __asm__ volatile("sti");
    while ((ev = keyboard_last()).code == KEY_NONE) {
      // serial_write_line("SYS_READ_KEY: halting...");
      schedule();
      //__asm__ volatile("hlt");
      // serial_write_line("SYS_READ_KEY: woke from hlt");
    }
    // serial_write_line("SYS_READ_KEY: got key!");
    __asm__ volatile("cli");
    return ((ulong)(ubyte)ev.character << 32) | (ulong)(uint)ev.code;
  }

  case SYS_EXEC: {
    fat32_file *f = fat32_open((const char *)arg1);
    if (!f)
      return (ulong)-1;
    task *t = elf_load(f);
    if (!t)
      return (ulong)-1;
    scheduler_add(t);
    return (ulong)t->pid;
  }

  case SYS_WAIT: {
    task *t = scheduler_find((int)arg1);
    if (!t)
      return (ulong)-1;
    while (t->state != TASK_DEAD)
      schedule();
    return 0;
  }

  default:
    return (ulong)-1;
  }
}
