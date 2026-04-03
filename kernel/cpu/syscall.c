#include "syscall.h"
#include "filesystem/elf.h"
#include "filesystem/fat32.h"
#include "gdt.h"
#include "graphics/console.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "io/serial.h"
#include "io/time.h"
#include "ipc/pipe.h"
#include "memory/vmm.h"
#include "scheduler/scheduler.h"
#include "scheduler/task.h"
#include "syscalls.h"
#include "types.h"

// Framebuffer globals defined in kernel.c
extern uint g_fb_width;
extern uint g_fb_height;
extern uint g_fb_pitch;
extern ulong g_fb_phys;

// Fixed user virtual address where the framebuffer is mapped via SYS_MAP_FB
#define USER_FB_VADDR 0x0000600000000000ULL

// Layout of the fb_info struct in userspace (must match user/lib/gfx.h)
typedef struct {
  ulong ptr; // user virtual address of framebuffer
  uint width;
  uint height;
  uint pitch; // bytes per scanline
} kernel_fb_info;

static int handle_alloc(task *t, handle_type type, void *ptr) {
  for (int i = 0; i < MAX_HANDLES; i++) {
    if (t->handles[i].type == HANDLE_NONE) {
      t->handles[i].type = type;
      t->handles[i].ptr = ptr;
      return i;
    }
  }
  return -1;
}

static handle_entry *handle_get(task *t, int fd) {
  if (fd < 0 || fd >= MAX_HANDLES)
    return 0;
  if (t->handles[fd].type == HANDLE_NONE)
    return 0;
  return &t->handles[fd];
}

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

  case SYS_WRITE_HEX:
    serial_write_hex(arg1);
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
    task *parent = scheduler_current();

    if (!f)
      return (ulong)-1;
    task *t = elf_load(f, "", parent->working_directory);
    if (!t)
      return (ulong)-1;
    // Inherit stdin/stdout handles if provided (arg2=stdin_fd, arg3=stdout_fd)

    if (arg2 != (ulong)-1) {
      handle_entry *h = handle_get(parent, (int)arg2);
      if (h)
        t->handles[0] = *h;
    }
    if (arg3 != (ulong)-1) {
      handle_entry *h = handle_get(parent, (int)arg3);
      if (h)
        t->handles[1] = *h;
    }
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

  case SYS_PIPE: {
    pipe *p = pipe_create();
    if (!p)
      return (ulong)-1;
    task *t = scheduler_current();
    int read_fd = handle_alloc(t, HANDLE_PIPE_READ, p);
    int write_fd = handle_alloc(t, HANDLE_PIPE_WRITE, p);
    if (read_fd < 0 || write_fd < 0)
      return (ulong)-1;
    pipe_retain(p); // now referenced by both ends
    return ((ulong)write_fd << 32) | (ulong)read_fd;
  }

  case SYS_READ_FD: {
    task *t = scheduler_current();
    handle_entry *h = handle_get(t, (int)arg1);
    if (!h || h->type != HANDLE_PIPE_READ)
      return (ulong)-1;
    pipe *p = (pipe *)h->ptr;
    __asm__ volatile("sti");
    while (pipe_available(p) == 0)
      schedule();
    __asm__ volatile("cli");
    return (ulong)pipe_read(p, (ubyte *)arg2, (uint)arg3);
  }

  case SYS_WRITE_FD: {
    task *t = scheduler_current();
    handle_entry *h = handle_get(t, (int)arg1);
    if (!h || h->type != HANDLE_PIPE_WRITE)
      return (ulong)-1;
    pipe *p = (pipe *)h->ptr;
    pipe_write(p, (const ubyte *)arg2, (uint)arg3);
    return arg3;
  }

  case SYS_MAP_FB: {
    task *t = scheduler_current();
    ulong fb_size = (ulong)g_fb_height * g_fb_pitch;
    vmm_map_bytes(t->address_space, USER_FB_VADDR, g_fb_phys, fb_size,
                  PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    kernel_fb_info *info = (kernel_fb_info *)arg1;
    info->ptr = USER_FB_VADDR;
    info->width = g_fb_width;
    info->height = g_fb_height;
    info->pitch = g_fb_pitch;
    return 0;
  }

  case SYS_PIPE_AVAIL: {
    task *t = scheduler_current();
    handle_entry *h = handle_get(t, (int)arg1);
    if (!h || h->type != HANDLE_PIPE_READ)
      return (ulong)-1;
    return (ulong)pipe_available((pipe *)h->ptr);
  }

  case SYS_READ_KEY_NB: {
    KeyEvent ev = keyboard_last();
    if (ev.code == KEY_NONE)
      return 0;
    return ((ulong)(ubyte)ev.character << 32) | (ulong)(uint)ev.code;
  }

  case SYS_YIELD:
    schedule();
    return 0;

  case SYS_TIME:
    return time_now();

  default:
    return (ulong)-1;
  }
}
