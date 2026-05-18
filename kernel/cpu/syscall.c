#include "syscall.h"
#include "compositor/window_manager.h"
#include "cpu_info.h"
#include "fb_info.h"
#include "filesystem/file.h"
#include "gdt.h"
#include "graphics/console.h"
#include "io/e1000.h"
#include "io/keyboard.h"
#include "io/mouse.h"
#include "io/serial.h"
#include "io/time.h"
#include "ipc/pipe.h"
#include "keys.h"
#include "mem_info.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "net/dhcp.h"
#include "net/socket.h"
#include "net_types.h"
#include "process_info.h"
#include "scheduler/process.h"
#include "scheduler/process_manager.h"
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
// typedef struct {
//  ulong ptr; // user virtual address of framebuffer
//  uint width;
//  uint height;
//  uint pitch; // bytes per scanline
//} kernel_fb_info;

static int handle_alloc(task *t, handle_type type, void *ptr) {
  for (int i = 0; i < MAX_HANDLES; i++) {
    if (t->owner->handles[i].type == HANDLE_NONE) {
      t->owner->handles[i].type = type;
      t->owner->handles[i].ptr = ptr;
      return i;
    }
  }
  return -1;
}

static handle_entry *handle_get(task *t, int fd) {
  if (fd < 0 || fd >= MAX_HANDLES)
    return 0;
  if (t->owner->handles[fd].type == HANDLE_NONE)
    return 0;
  return &t->owner->handles[fd];
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
    while ((ev = keyboard_last()).code == KEY_NONE)
      __asm__ volatile("sti; hlt; cli");
    return ((ulong)(ubyte)ev.character << 32) | (ulong)(uint)ev.code;
  }

  case SYS_EXEC: {
    return process_exec((const char *)arg1, (int)arg2, (int)arg3);
  }

  case SYS_WAIT: {
    if (arg1) {
      task *t = scheduler_find((int)arg1);
      if (!t)
        return (ulong)-1;
      while (t->state != TASK_DEAD)
        schedule();
      return arg1;
    } else {
      task *current_task = scheduler_current();
      process *current_proc = current_task->owner;
      if (!current_proc->first_child)
        return 0;
      while (1) {
        for (process *child = current_proc->first_child; child; child = child->next_sibling) {
          if (child->main_task->state == TASK_DEAD) {
            return child->pid;
          }
        }
        schedule();
      }
    }
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
    while (pipe_available(p) == 0)
      __asm__ volatile("sti; hlt; cli");
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
    vmm_map_bytes(t->owner->address_space, USER_FB_VADDR, g_fb_phys, fb_size,
                  PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    fb_info *info = (fb_info *)arg1;
    info->ptr = (uint *)USER_FB_VADDR;
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

  case SYS_READ_MOUSE: {
    mouse_state ms = mouse_read_state();
    if (!ms.pending)
      return 0;
    return ((ulong)1 << 48) | ((ulong)(ubyte)(char)ms.scroll << 49) |
           ((ulong)ms.buttons << 32) |
           ((ulong)(ushort)ms.y << 16) | (ushort)ms.x;
  }

  case SYS_ALLOC: {
    task *t = scheduler_current();
    ulong size = arg1;
    if (size == 0)
      return 0;
    ulong pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    ulong phys = pmm_alloc_pages(pages);
    if (!phys)
      return 0;
    ulong vaddr = t->owner->heap_next;
    vmm_map_pages(t->owner->address_space, vaddr, phys, pages,
                  PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    t->owner->heap_next += pages * PAGE_SIZE;
    return vaddr;
  }

  case SYS_FREE: {
    ulong vaddr = arg1;
    ulong size = arg2;
    if (!vaddr || !size)
      return 0;
    task *t = scheduler_current();
    ulong pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    vmm_unmap_and_free_pages(t->owner->address_space, vaddr, pages);
    return 0;
  }

  case SYS_YIELD:
    schedule();
    return 0;

  case SYS_UNIX_TIME:
    return time_unix();

  case SYS_UNIX_TIME_MILLIS:
    return time_unix_millis();

  case SYS_VBLANK_WAIT: {
    ulong now = time_unix_millis();
    ulong next = ((now / 16) + 1) * 16; // next 16ms boundary (~60 FPS)
    __asm__ volatile("sti");
    while (time_unix_millis() < next)
      schedule();
    __asm__ volatile("cli");
    return 0;
  }

  case SYS_PROCESS_EXEC:
    return process_exec((const char *)arg1, (int)arg2, (int)arg3);

  case SYS_PROCESS_LIST:
    return process_list((process_info *)arg1, arg2);

  case SYS_FILE_OPEN:
    return (ulong)file_open((const char *)arg1);
  case SYS_FILE_CLOSE:
    file_close((file_handle)arg1);
    return 0;
  case SYS_FILE_READDIR:
    return file_readdir((const char *)arg1, (file_dirent *)arg2, arg3);
  case SYS_FILE_READ:
    return file_read((file_handle)arg1, (void *)arg2, arg3);

  case SYS_FILE_SIZE:
    return ((file_handle)arg1)->size;

  case SYS_SOCKET_UDP:
    return socket_udp((ipv4_addr)(uint)arg1, (ushort)arg2, (ushort)arg3).value;
  case SYS_NET_GET_MAC:
    e1000_get_mac((mac_addr *)arg1);
    return 0;
  case SYS_NET_SET_IP:
    g_ip = (ipv4_addr)(uint)arg1;
    return 0;

  case SYS_SOCKET_CONNECT:
    return socket_tcp_client((ipv4_addr)(uint)arg1, (arg2), 0).value;
  case SYS_SOCKET_LISTEN:
    return socket_tcp_server(arg1).value;
  case SYS_SOCKET_ACCEPT:
    return socket_accept((socket_handle)(uint)arg1).value;
  case SYS_SOCKET_RECEIVE:
    return socket_recv((socket_handle)(uint)arg1, (void *)arg2, arg3);
  case SYS_SOCKET_SEND:
    return socket_send((socket_handle)(uint)arg1, (void *)arg2, arg3);
  case SYS_SOCKET_CLOSE:
    socket_close((socket_handle)(uint)arg1);
    return 0;

  case SYS_WM_REGISTER: {
    task *t = scheduler_current();
    return (ulong)wm_register(t);
  }

  case SYS_WM_POLL: {
    // arg1 = pointer to wm in user space
    return (ulong)wm_poll((wm_event *)arg1);
  }

  case SYS_WINDOW_CREATE: {
    // arg1=window_create_options*; returns window_handle or 0
    typedef struct {
      ushort width;
      ushort height;
      const char *title;
    } wc_opts;
    wc_opts *opts = (wc_opts *)arg1;
    task *t = scheduler_current();
    window_handle handle = 0;
    ulong vaddr = wm_window_create(t, opts->width, opts->height,
                                   opts->title, &handle);
    if (!vaddr)
      return (ulong)-1;
    return handle;
  }

  case SYS_WINDOW_PRESENT:
    wm_present_window((window_handle)arg1);
    return 0;

  case SYS_WINDOW_POST_EVENT:
    return (ulong)wm_post_event((window_handle)arg1, (window_event *)arg2);

  case SYS_WINDOW_POLL:
    return (ulong)wm_window_poll_event((window_handle)arg1, (window_event *)arg2);

  case SYS_WINDOW_FRAMEBUFFER: {
    wm_get_framebuffer(arg1, (fb_info *)arg2);
    return 0;
  }

  case SYS_STATS_MEMORY: {
    mem_info *info = (mem_info *)arg1;
    pmm_get_stats(info);
    return 0;
  }

  case SYS_STATS_CPU: {
    cpu_info *info = (cpu_info *)arg1;
    uint eax, ebx, ecx, edx;

    // Brand string: three CPUID leaves, 16 bytes each
    for (int i = 0; i < 3; i++) {
      uint *p = (uint *)(info->brand + i * 16);
      __asm__ volatile("cpuid"
                       : "=a"(p[0]), "=b"(p[1]), "=c"(p[2]), "=d"(p[3])
                       : "a"(0x80000002 + i), "c"(0));
    }
    info->brand[48] = '\0';

    // Base and max MHz (CPUID leaf 0x16, Intel Skylake+ / modern AMD)
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x16), "c"(0));
    info->base_mhz = eax & 0xFFFF;
    info->max_mhz = ebx & 0xFFFF;

    // Logical core count: EBX[23:16] is only valid when HTT (EDX[28]) is set
    __asm__ volatile("cpuid"
                     : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
                     : "a"(0x1), "c"(0));
    int cores = (edx & (1 << 28)) ? (int)((ebx >> 16) & 0xFF) : 0;
    info->logical_cores = cores > 0 ? cores : 1;

    return 0;
  }

  default:
    return (ulong)-1;
  }
}
