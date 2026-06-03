#include "../shared/boot_info.h"
#include "acpi/acpi.h"
#include "acpi/battery.h"
#include "cpu/exceptions.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/syscall.h"
#include "filesystem/filesystem.h"
#include "graphics/console.h"
#include "graphics/gfx.h"
#include "graphics/image.h"
#include "io/keyboard.h"
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
#include "keys.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "net/networking.h"
#include "panic.h"
#include "scheduler/process_manager.h"
#include "scheduler/scheduler.h"
#include <stddef.h>

uint g_fb_width;
uint g_fb_height;
uint g_fb_pitch;
ulong g_fb_phys;
ulong g_rsdp_phys;
BootDevice g_boot_device;
bool g_fast_boot;

extern void enter_kernel_main(ulong stack_addr);

void kernel_pre_main(BootInfo *boot_info) {
  serial_init();
  g_fb_width = boot_info->framebuffer_width;
  g_fb_height = boot_info->framebuffer_height;
  g_fb_pitch = boot_info->framebuffer_pitch;
  g_fb_phys = boot_info->framebuffer_base;
  g_boot_device = boot_info->boot_device;
  g_fast_boot = boot_info->fast_boot;
  g_rsdp_phys = boot_info->rsdp_phys;

  console_init(g_fb_phys, g_fb_width, g_fb_height, g_fb_pitch);
  console_write_line("Console initialized... ");

  pmm_init(boot_info);
  console_write_line("PMM initialized... ");
  serial_write("PMM initialized!\n");
  vmm_init(boot_info);
  // framebuffer now mapped at FB_BASE — switch to virtual address
  console_init(FB_BASE, g_fb_width, g_fb_height, g_fb_pitch);
  console_write_line("VMM initialized... ");
  pmm_remap_bitmap(0xFFFF800000000000ULL);
  serial_write("VMM initialized!\n");
  console_write_line("PMM remapped... ");
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
#ifdef KERNEL_PERF
  klogf(LOG_INFO, "Performance monitoring activated");
#endif

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

  acpi_init(g_rsdp_phys);
  battery_init();

  networking_init();
  klogf(LOG_INFO, "NETWORK LOGGING ENABLED");

  timer_init(1000);

  if (!g_fast_boot) {
    klogf(LOG_INFO, "Initializing USB...");
    usb_init();
    xhci_init();
  }

  if (fs_init(&g_boot_device)) {
    klogf(LOG_CRITICAL, "Failed to install file system!");
    for (;;)
      __asm__ volatile("hlt");
  }

  scheduler_init();
  klogf(LOG_DEBUG, "Scheduler initilized!");

  pid init_pid = process_exec("/sys/programs/system.elf", -1, -1, 0, 0);

  if (!init_pid) {
    panic("Failed to load init ELF");
  }
  klogf(LOG_DEBUG, "Tasks loaded!");

  rtc_time t;
  rtc_read(&t);
  serial_printf("%04u-%02u-%02u %02u:%02u:%02u\n",
                t.year, t.month, t.day, t.hour, t.minute, t.second);

  time_init();

  if (!g_fast_boot) {
    gfx_clear(RGB(82, 50, 49));
    bitmap *logo = img_load("/logo.lbm");
    if (logo) {
      gfx_img(100, 100, logo);
      kfree(logo);
    }
    ksleep_ms(4000);
  }
  klogf(LOG_INFO, "Starting scheduler...");
  logging_set_screen_logging(false);
  scheduler_run();

  klogf(LOG_INFO, "Goodbye from kernel!");
  for (;;)
    __asm__ volatile("hlt");
}