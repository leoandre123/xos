#include "syscall.h"
#include "acpi/battery.h"
#include "cpu_info.h"
#include "fb_info.h"
#include "filesystem/file.h"
#include "gdt.h"
#include "graphics/console.h"
#include "io/keyboard.h"
#include "io/mouse.h"
#include "io/serial.h"
#include "io/time.h"
#include "io/timer.h"
#include "ipc/channel.h"
#include "ipc/pipe.h"
#include "keys.h"
#include "mem_info.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "net/networking.h"
#include "net/routing.h"
#include "net/socket.h"
#include "net_types.h"
#include "nic_info.h"
#include "panic.h"
#include "perf/perf.h"
#include "process_info.h"
#include "route_info.h"
#include "scheduler/process.h"
#include "scheduler/process_manager.h"
#include "scheduler/scheduler.h"
#include "scheduler/task.h"
#include "syscalls.h"
#include "thread.h"
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

// static int handle_alloc(task *t, handle_type type, void *ptr) {
//   for (int i = 0; i < MAX_HANDLES; i++) {
//     if (t->owner->handles[i].type == HANDLE_NONE) {
//       t->owner->handles[i].type = type;
//       t->owner->handles[i].ptr = ptr;
//       return i;
//     }
//   }
//   return -1;
// }
//
// static handle_entry *handle_get(task *t, int fd) {
//   if (fd < 0 || fd >= MAX_HANDLES)
//     return 0;
//   if (t->owner->handles[fd].type == HANDLE_NONE)
//     return 0;
//   return &t->owner->handles[fd];
// }

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

ulong syscall_dispatch(ulong num, ulong arg1, ulong arg2, ulong arg3, ulong arg4, ulong arg5) {
  (void)arg2;
  (void)arg3;

  task *_ct = scheduler_current();
  PERF_MODE_SYSCALL_SCOPE(_ct);

  // serial_write("SYSCALL: ");
  // serial_write_hex8(num);
  // serial_write_char('\n');

  switch (num) {
  case SYS_WRITE: {
    PERF_SCOPE("SYS_WRITE");
    serial_write((const char *)arg1);
    return 0;
  }

  case SYS_WRITE_HEX: {
    PERF_SCOPE("SYS_WRITE_HEX");
    serial_write_hex(arg1);
    return 0;
  }
  case SYS_EXIT:
    process_exit(arg1);
    return 0;

  case SYS_WRITE_CONSOLE: {
    PERF_SCOPE("SYS_WRITE_CONSOLE");
    console_write((const char *)arg1);
    return 0;
  }

  case SYS_READ_KEY: {
    PERF_SCOPE("SYS_READ_KEY");
    KeyEvent ev = keyboard_read();
    if (ev.code != KEY_NONE)
      return ((ulong)(ubyte)ev.character << 32) | (ulong)(uint)ev.code;

    task *t = scheduler_current();

    if (!keyboard_set_reader(t))
      return 0;
    task_set_blocked(t);
    schedule();
    ev = keyboard_read();
    return ((ulong)(ubyte)ev.character << 32) | (ulong)(uint)ev.code;
  }

  case SYS_EXEC: {
    PERF_SCOPE("SYS_EXEC");
    return process_exec((const char *)arg1, (int)arg2, (int)arg3, (int)arg4, (const char **)arg5);
  }

  case SYS_WAIT: {
    PERF_SCOPE("SYS_WAIT");
    return process_wait((pid)arg1);
  }

  case SYS_PIPE: {
    PERF_SCOPE("SYS_PIPE");
    pipe *p = pipe_create();
    if (!p)
      return (ulong)-1;
    task *t = scheduler_current();
    int read_fd = handle_alloc(t->owner, HANDLE_PIPE_READ, p);
    int write_fd = handle_alloc(t->owner, HANDLE_PIPE_WRITE, p);
    if (read_fd < 0 || write_fd < 0)
      return (ulong)-1;
    pipe_retain(p); // now referenced by both ends
    return ((ulong)write_fd << 32) | (ulong)read_fd;
  }

  // TODO: Still blocking
  case SYS_READ_FD: {
    PERF_SCOPE("SYS_READ_FD");
    task *t = scheduler_current();
    handle_entry *h = handle_get(t->owner, (int)arg1);
    if (!h || h->type != HANDLE_PIPE_READ)
      return (ulong)-1;
    pipe *p = (pipe *)h->ptr;

    ulong read = (ulong)pipe_read(p, (ubyte *)arg2, (uint)arg3);

    if (read)
      return read;

    if (!pipe_add_listener(p, t)) {
      return 0;
    }
    task_set_blocked(t);
    schedule();
    return (ulong)pipe_read(p, (ubyte *)arg2, (uint)arg3);
  }

  case SYS_WRITE_FD: {
    PERF_SCOPE("SYS_WRITE_FD");
    task *t = scheduler_current();
    handle_entry *h = handle_get(t->owner, (int)arg1);
    if (!h || h->type != HANDLE_PIPE_WRITE)
      return (ulong)-1;
    pipe *p = (pipe *)h->ptr;
    pipe_write(p, (const ubyte *)arg2, (uint)arg3);
    return arg3;
  }

  case SYS_MAP_FB: {
    PERF_SCOPE("SYS_MAP_FB");
    task *t = scheduler_current();
    ulong fb_size = (ulong)g_fb_height * g_fb_pitch;
    vmm_map_bytes(t->owner->address_space, USER_FB_VADDR, g_fb_phys, fb_size,
                  PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_WRITE_COMBINING);
    fb_info *info = (fb_info *)arg1;
    info->ptr = (uint *)USER_FB_VADDR;
    info->width = g_fb_width;
    info->height = g_fb_height;
    info->pitch = g_fb_pitch;
    return 0;
  }

  case SYS_PIPE_AVAIL: {
    PERF_SCOPE("SYS_PIPE_AVAIL");
    task *t = scheduler_current();
    handle_entry *h = handle_get(t->owner, (int)arg1);
    if (!h || h->type != HANDLE_PIPE_READ)
      return (ulong)-1;
    return (ulong)pipe_available((pipe *)h->ptr);
  }

  case SYS_READ_KEY_NB: {
    PERF_SCOPE("SYS_READ_KEY_NB");
    KeyEvent ev = keyboard_read();
    if (ev.code == KEY_NONE)
      return 0;
    return ((ulong)(ubyte)ev.character << 32) | (ulong)(uint)ev.code;
  }

  case SYS_READ_MOUSE: {
    PERF_SCOPE("SYS_READ_MOUSE");
    mouse_state ms = mouse_read_state();
    if (!ms.pending)
      return 0;
    return ((ulong)1 << 48) | ((ulong)(ubyte)(char)ms.scroll << 49) |
           ((ulong)ms.buttons << 32) |
           ((ulong)(ushort)ms.y << 16) | (ushort)ms.x;
  }

  case SYS_ALLOC: {
    PERF_SCOPE("SYS_ALLOC");
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
    PERF_SCOPE("SYS_FREE");
    ulong vaddr = arg1;
    ulong size = arg2;
    if (!vaddr || !size)
      return 0;
    task *t = scheduler_current();
    ulong pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    vmm_unmap_and_free_pages(t->owner->address_space, vaddr, pages);
    return 0;
  }

  // arg1=size, arg2=ch handle, arg3=[OUT]client_vaddr
  case SYS_ALLOC_SHARED: {
    PERF_SCOPE("SYS_ALLOC_SHARED");
    task *t = scheduler_current();
    ulong size = arg1;
    if (size == 0)
      return 0;

    handle_entry *handle = handle_get(t->owner, arg2);
    if (!handle)
      return 0;
    channel *ch = handle->ptr;
    if (!ch)
      return 0;
    if (ch->pids[1] != t->owner->pid)
      return 0;
    process *client = find_process(ch->pids[0]);
    if (!client)
      return 0;

    ulong pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    ulong phys = pmm_alloc_pages(pages);
    if (!phys)
      return 0;

    ulong vaddr = t->owner->heap_next;
    vmm_map_pages(t->owner->address_space, vaddr, phys, pages,
                  PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    t->owner->heap_next += pages * PAGE_SIZE;

    ulong client_vaddr = client->heap_next;
    vmm_map_pages(client->address_space, client_vaddr, phys, pages,
                  PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER);
    client->heap_next += pages * PAGE_SIZE;

    *((ulong *)arg3) = client_vaddr;
    return vaddr;
  }

  case SYS_YIELD: {
    PERF_SCOPE("SYS_YIELD");
    schedule();
    return 0;
  }

  case SYS_UNIX_TIME: {
    PERF_SCOPE("SYS_UNIX_TIME");
    return time_unix();
  }

  case SYS_UNIX_TIME_MILLIS: {
    PERF_SCOPE("SYS_UNIX_TIME_MILLIS");
    return time_unix_millis();
  }

  case SYS_SLEEP: {
    PERF_SCOPE("SYS_SLEEP");
    ulong now = timer_get_ticks();
    ulong wakeup_tick = now + timer_ms_to_ticks(arg1);
    task *t = scheduler_current();
    if (sleep_queue_enqueue(t, wakeup_tick)) {
      task_set_blocked(t);
    }
    schedule();
    return 0;
  }

  case SYS_PROCESS_EXEC: {
    PERF_SCOPE("SYS_PROCESS_EXEC");
    return process_exec((const char *)arg1, (int)arg2, (int)arg3, (int)arg4, (const char **)arg5);
  }
  case SYS_PROCESS_LIST: {
    PERF_SCOPE("SYS_PROCESS_LISY");
    return process_list((process_info *)arg1, arg2);
  }
#pragma region threads

  case SYS_THREAD: {
    PERF_SCOPE("SYS_THREAD");
    return process_spawn_thread((void(*))arg1, arg2);
  }
  case SYS_THREAD_JOIN: {
    PERF_SCOPE("SYS_THREAD_JOIN");
    return process_join_thread((thread_handle)arg1);
  }
  case SYS_THREAD_KILL: {
    PERF_SCOPE("SYS_THREAD_KILL");
    process_kill_thread((thread_handle)arg1);
    return 0;
  }

  case SYS_THREAD_EXIT: {
    process_exit_thread(arg1);
  }
#pragma endregion threads

  case SYS_FILE_OPEN: {
    PERF_SCOPE("SYS_FILE_OPEN");
    return (ulong)file_open((const char *)arg1);
  }
  case SYS_FILE_CLOSE: {
    PERF_SCOPE("SYS_FILE_CLOSE");
    file_close((file_handle)arg1);
    return 0;
  }
  case SYS_FILE_READDIR: {
    PERF_SCOPE("SYS_FILE_READDIR");
    return file_readdir((const char *)arg1, (file_dirent *)arg2, arg3);
  }
  case SYS_FILE_READ: {
    PERF_SCOPE("SYS_FILE_READ");
    return file_read((file_handle)arg1, (void *)arg2, arg3);
  }
  case SYS_FILE_SIZE: {
    PERF_SCOPE("SYS_FILE_SIZE");
    return ((file_handle)arg1)->size;
  }

  case SYS_SOCKET: {
    PERF_SCOPE("SYS_SOCKET");
    return socket(arg1).value_ulong;
  }
  case SYS_SOCKET_CLOSE: {
    PERF_SCOPE("SYS_SOCKET_CLOSE");
    socket_close((socket_handle)(uint)arg1);
    return 0;
  }
  case SYS_SOCKET_BIND: {
    PERF_SCOPE("SYS_SOCKET_BIND");
    return (ulong)socket_bind((socket_handle)(uint)arg1, (socket_addr *)arg2);
  }
  case SYS_SOCKET_BIND_NIC: {
    PERF_SCOPE("SYS_SOCKET_BIND_NIC");
    return (ulong)socket_bind_nic((socket_handle)(uint)arg1, (int)arg2);
  }
  case SYS_SOCKET_LISTEN: {
    PERF_SCOPE("SYS_SOCKET_LISTEN");
    return socket_listen((socket_handle)arg1);
  }
  case SYS_SOCKET_ACCEPT: {
    PERF_SCOPE("SYS_SOCKET_ACCEPT");
    return socket_accept((socket_handle)arg1).value_ulong;
  }
  case SYS_SOCKET_CONNECT: {
    PERF_SCOPE("SYS_SOCKET_CONNECT");
    return socket_connect((socket_handle)arg1, (socket_addr *)arg2);
  }
  case SYS_SOCKET_SEND: {
    PERF_SCOPE("SYS_SOCKET_SEND");
    return socket_send((socket_handle)arg1, (void *)arg2, arg3);
  }
  case SYS_SOCKET_RECEIVE: {
    PERF_SCOPE("SYS_SOCKET_RECEIVE");
    int read_len = socket_recv((socket_handle)arg1, (void *)arg2, arg3);
    if (read_len)
      return read_len;
    task *t = scheduler_current();
    if (!socket_set_receiver((socket_handle)arg1, t)) {
      return 0;
    }
    task_set_blocked(t);
    schedule();
    return socket_recv((socket_handle)arg1, (void *)arg2, arg3);
  }
  case SYS_SOCKET_RECEIVE_NB: {
    PERF_SCOPE("SYS_SOCKET_RECEIVE_NB");
    return socket_recv((socket_handle)arg1, (void *)arg2, arg3);
  }
  case SYS_NET_GET_MAC: {
    PERF_SCOPE("SYS_NET_GET_MAC");
    if (arg1 < MAX_NICS)
      g_nics[arg1].driver->get_mac((mac_addr *)arg2);
    return 0;
  }

  case SYS_NET_SET_IP: {
    if (arg1 < MAX_NICS)
      g_nics[arg1].addr = (ipv4_addr)(uint)arg2;
    return 0;
  }

  case SYS_NET_NICS: {
    PERF_SCOPE("SYS_NET_NICS");
    int c = 0;
    for (int i = 0; i < MAX_NICS; i++) {
      if (g_nics[i].nic_id) {
        nic_info *info = &((nic_info *)arg1)[c++];
        info->nic_id = g_nics[i].nic_id;
        info->addr = g_nics[i].addr;
        info->netmask = g_nics[i].netmask;
        info->default_gateway = g_nics[i].default_gateway;
        info->is_up = g_nics[i].is_up;
        if (c >= arg2) {
          break;
        }
      }
    }
    return c;
  }
  case SYS_NET_ROUTES: {
    PERF_SCOPE("SYS_NET_ROUTES");
    return get_routes((route_info *)arg1, arg2);
  }

  case SYS_NET_CONF_NIC: {
    PERF_SCOPE("SYS_NET_CONF_NIC");
    configure_nic(arg1, arg2, arg3);
    return 0;
  }

  case SYS_STATS_MEMORY: {
    PERF_SCOPE("SYS_STATS_MEMORY");
    mem_info *info = (mem_info *)arg1;
    pmm_get_stats(info);
    return 0;
  }

  case SYS_STATS_CPU: {
    PERF_SCOPE("SYS_STATS_CPU");
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

  case SYS_BATTERY_INFO: {
    PERF_SCOPE("SYS_BATTERY_INFO");
    battery_info *info = (battery_info *)arg2;
    return battery_get((uint)arg1, info) ? 1 : 0;
  }

  case SYS_IPC_SERVER: {
    PERF_SCOPE("SYS_IPC_SERVER");
    return ipc_server((const char *)arg1);
  }
  case SYS_IPC_SERVER_ACCEPT: {
    PERF_SCOPE("SYS_IPC_SERVER_ACCEPT");
    return ipc_accept(arg1);
  }
  case SYS_IPC_SERVER_ACCEPT_NB: {
    PERF_SCOPE("SYS_IPC_SERVER_ACCEPT");
    return ipc_accept_nb(arg1);
  }
  case SYS_IPC_SERVER_CONNECT: {
    PERF_SCOPE("SYS_IPC_SERVER_CONNECT");
    return ipc_connect((const char *)arg1);
  }
  case SYS_IPC_SEND: {
    PERF_SCOPE("SYS_IPC_SEND");
    channel_send(arg1, (const void *)arg2, arg3);
    return 0;
  }
  case SYS_IPC_RECV: {
    PERF_SCOPE("SYS_IPC_RECV");

    int read_len = channel_recv(arg1, (void *)arg2, arg3);
    if (read_len)
      return read_len;

    task *t = scheduler_current();
    if (!channel_set_listener(arg1, t)) {
      return 0;
    }
    task_set_blocked(t);
    schedule();
    return channel_recv(arg1, (void *)arg2, arg3);
  }
  case SYS_IPC_RECV_NB: {
    PERF_SCOPE("SYS_IPC_RECV_NB");
    return channel_recv(arg1, (void *)arg2, arg3);
  }

  default:
    panic("Invalid syscall number");
    return (ulong)-1;
  }
}
