#include "../shared/boot_info.h"
#include "cpu/exceptions.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/syscall.h"
#include "filesystem/elf.h"
#include "filesystem/fat32.h"
#include "filesystem/file.h"
#include "filesystem/filesystem.h"
#include "graphics/console.h"
#include "graphics/gfx.h"
#include "io/ata.h"
#include "io/e1000.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "io/logging.h"
#include "io/mouse.h"
#include "io/pci.h"
#include "io/pic.h"
#include "io/rtc.h"
#include "io/serial.h"
#include "io/time.h"
#include "io/timer.h"
#include "io/usb/usb.h"
#include "io/xhci.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "net/dhcp.h"
#include "net/dns.h"
#include "net/icmp.h"
#include "net/net.h"
#include "net/networking.h"
#include "panic.h"
#include "scheduler/scheduler.h"
#include <stddef.h>

uint g_fb_width;
uint g_fb_height;
uint g_fb_pitch;
ulong g_fb_phys;
BootDevice g_boot_device;

extern void enter_kernel_main(ulong stack_addr);

static void fb_bar(uint *fb, uint pitch_px, uint width, uint height, uint y_bar, uint color) {
  for (uint y = y_bar; y < y_bar + 32 && y < height; y++)
    for (uint x = 0; x < width; x++)
      fb[y * pitch_px + x] = color;
}

void kernel_pre_main(BootInfo *boot_info) {
  serial_init();
  g_fb_width = boot_info->framebuffer_width;
  g_fb_height = boot_info->framebuffer_height;
  g_fb_pitch = boot_info->framebuffer_pitch;
  g_fb_phys = boot_info->framebuffer_base;
  g_boot_device = boot_info->boot_device;

  uint *fb_phys = (uint *)(ulong)boot_info->framebuffer_base;
  uint pitch_px = boot_info->framebuffer_pitch / 4;
  console_init(g_fb_phys, g_fb_width, g_fb_height, g_fb_pitch);
  fb_bar(fb_phys, pitch_px, g_fb_width, g_fb_height, 0, 0x0000FF); // blue   = kernel reached
  console_write_line("Console initialized... ");

  pmm_init(boot_info);
  fb_bar(fb_phys, pitch_px, g_fb_width, g_fb_height, 32, 0xFFFF00); // yellow = PMM done
  console_write_line("PMM initialized... ");
  serial_write("PMM initialized!\n");
  vmm_init(boot_info);
  // framebuffer now mapped at FB_BASE — switch to virtual address
  uint *fb_virt = (uint *)FB_BASE;
  console_init(FB_BASE, g_fb_width, g_fb_height, g_fb_pitch);
  console_write_line("VMM initialized... ");
  fb_bar(fb_virt, pitch_px, g_fb_width, g_fb_height, 64, 0x00FF80); // green  = VMM done
  pmm_remap_bitmap(0xFFFF800000000000ULL);
  serial_write("VMM initialized!\n");
  console_write_line("PMM remapped... ");
  fb_bar(fb_virt, pitch_px, g_fb_width, g_fb_height, 96, 0xFF8000); // orange = entering kernel_main
  console_write_line("Ready to enter kernel main... ");

  enter_kernel_main(KERNEL_STACK_TOP);

  panic("This should never be reached: 'enter_kernel_main' has returned");
  for (;;)
    asm volatile("hlt");
}

void kernel_main() {
  heap_init();

  *((ulong *)FB_BASE) = 0x00ff0000;

  gfx_init((uint *)FB_BASE, g_fb_width, g_fb_height, g_fb_pitch);
  gfx_clear(0xa8328b);
  console_init(FB_BASE, g_fb_width, g_fb_height, g_fb_pitch);
  console_set_bg_color(0xa8328b);
  klogf(LOG_INFO, "LeOS!");
  klogf(LOG_INFO, "Hello from kernel!");

  gdt_init();
  klogf(LOG_INFO, "GDT initialized...");
  syscall_init();
  klogf(LOG_INFO, "SYSCALL initialized...");
  idt_init();
  klogf(LOG_INFO, "IDT initialized...");

  pic_remap(32, 40);
  pic_mask_all();
  pic_unmask_irq(0);
  pic_unmask_irq(1);
  pic_unmask_irq(2);

  klogf(LOG_INFO, "PIC initialized...");

  exceptions_init();
  klogf(LOG_INFO, "EXCEPTIONS initialized...");

  keyboard_init();
  klogf(LOG_INFO, "Keyboard initialized...");
  mouse_init();
  klogf(LOG_INFO, "Mouse initialized...");
  pic_unmask_irq(12);

  klogf(LOG_INFO, "Boot device type: %d", g_boot_device.type);
  if (g_boot_device.type == BOOT_DEVICE_IDE) {
    klogf(LOG_DEBUG, "Boot device PCI: %02x:%02x:%02x", g_boot_device.pci.bus, g_boot_device.pci.dev, g_boot_device.pci.func);
  } else if (g_boot_device.type == BOOT_DEVICE_USB) {
    klogf(LOG_DEBUG, "Boot device USB: %d->%d->%d->%d", g_boot_device.usb.port_path[0], g_boot_device.usb.port_path[1], g_boot_device.usb.port_path[2], g_boot_device.usb.port_path[3]);
  }

  klogf(LOG_DEBUG, "ENABLING STI...");
  asm volatile("sti");
  klogf(LOG_DEBUG, "Running PCI Scan");
  pci_scan();

  networking_init();
  // Windows drops packets with src 0.0.0.0 before they reach Python/nc.
  // Set XOS to any unused IP on your ethernet adapter's subnet.
  // Windows adapter: Control Panel → ethernet → IPv4 → 192.168.100.1 / 255.255.255.0
  g_ip = ipv4(192, 168, 215, 200);
  logging_enable_network(ipv4(255, 255, 255, 255), 9999);

  klogf(LOG_INFO, "NETWORK LOGGING ENABLED");

  klogf(LOG_INFO, "Initializing USB...");
  usb_init();

  xhci_init();

  if (fs_init(&g_boot_device)) {
    klogf(LOG_CRITICAL, "Failed to install file system!");
    for (;;)
      __asm__ volatile("hlt");
  }

  scheduler_init();
  klogf(LOG_DEBUG, "Scheduler initilized!");

  // Read /init to pick the first process ("d" → dafne, else terminal)
  const char *init_elf = "/terminal.elf";
  const char *init_name = "terminal";
  file_handle init_cfg = file_open("/init");
  if (init_cfg) {
    char first = 0;
    file_read(init_cfg, &first, 1);
    if (first == 'd') {
      init_elf = "/dafne.elf";
      init_name = "dafne";
    }
    klogf(LOG_TRACE, "Init letter: %c(%d)", first, first);
    klogf(LOG_INFO, "Init file loaded. Configuration: %s", init_name);
  } else {
    klogf(LOG_WARNING, "Failed to load init file");
  }

  file_handle term_file = file_open(init_elf);
  if (!term_file)
    panic("Cannot find init ELF on disk");
  task *term_task = elf_load(term_file, init_name, "/");
  if (!term_task)
    panic("Failed to load init ELF");
  klogf(LOG_DEBUG, "Tasks created!");

  scheduler_add(term_task);

  klogf(LOG_DEBUG, "Tasks loaded!");
  // scheduler_add(user_task);

  rtc_time t;
  rtc_read(&t);
  serial_printf("%04u-%02u-%02u %02u:%02u:%02u\n",
                t.year, t.month, t.day, t.hour, t.minute, t.second);

  dhcp_send_discovery();

  klogf(LOG_DEBUG, "Sleep...");
  timer_init(47);

  ksleep_ms(4000);
  klogf(LOG_DEBUG, "Done!");

  dns_resolve("www.google.com");

  icmp_send_ping(IP(10, 0, 2, 2));
  icmp_send_ping(IP(8, 8, 8, 8));

  // http_test();

  time_init();

  logging_set_screen_logging(false);
  scheduler_run();

  klogf(LOG_INFO, "Goodbye from kernel!");
  for (;;)
    __asm__ volatile("hlt");
}