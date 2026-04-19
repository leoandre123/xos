#include "../shared/boot_info.h"
#include "cpu/exceptions.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/syscall.h"
#include "filesystem/elf.h"
#include "filesystem/fat32.h"
#include "filesystem/file.h"
#include "graphics/console.h"
#include "graphics/gfx.h"
#include "io/ata.h"
#include "io/e1000.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "io/mouse.h"
#include "io/pci.h"
#include "io/pic.h"
#include "io/rtc.h"
#include "io/serial.h"
#include "io/time.h"
#include "io/timer.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "net/dhcp.h"
#include "net/dns.h"
#include "net/icmp.h"
#include "panic.h"
#include "scheduler/scheduler.h"
#include <stdbool.h>
#include <stddef.h>

uint g_fb_width;
uint g_fb_height;
uint g_fb_pitch;
ulong g_fb_phys;

extern void enter_kernel_main(ulong stack_addr);

void kernel_pre_main(BootInfo *boot_info) {
  serial_init();
  g_fb_width = boot_info->framebuffer_width;
  g_fb_height = boot_info->framebuffer_height;
  g_fb_pitch = boot_info->framebuffer_pitch;
  g_fb_phys = boot_info->framebuffer_base;

  pmm_init(boot_info);
  serial_write("PMM initilized!\n");
  vmm_init(boot_info);
  pmm_remap_bitmap(0xFFFF800000000000ULL);
  serial_write("VMM initilized!\n");

  enter_kernel_main(KERNEL_STACK_TOP);

  panic("This should never be reached: 'enter_kernel_main' has returned");
  for (;;)
    asm volatile("hlt");
}

void kernel_main() {
  serial_write_line("LeOS!");
  serial_write_line("Hello from kernel!");

  heap_init();

  *((ulong *)FB_BASE) = 0x00ff0000;

  gfx_init((uint *)FB_BASE, g_fb_width, g_fb_height, g_fb_pitch);
  console_init(FB_BASE, g_fb_width, g_fb_height, g_fb_pitch);
  console_write_line("Hello, World!");

  gdt_init();
  syscall_init();
  idt_init();

  pic_remap(32, 40);
  pic_mask_all();
  pic_unmask_irq(0);
  pic_unmask_irq(1);
  pic_unmask_irq(2); // cascade — required for any PIC2 (IRQ8-15) to fire

  exceptions_init();
  keyboard_init();
  mouse_init();
  pic_unmask_irq(12);

  ata_init();
  fat32_init(0);
  fat32_print_root();

  serial_write("ENABLING STI...");
  asm volatile("sti");
  // asm volatile("cli");
  serial_write_line("DONE!");
  asm volatile("int $32");
  asm volatile("int $33");

  // asm volatile("int $3");

  scheduler_init();
  serial_write_line("Scheduler initilized!");

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
  }

  file_handle term_file = file_open(init_elf);
  if (!term_file)
    panic("Cannot find init ELF on disk");
  task *term_task = elf_load(term_file, init_name, "/");
  if (!term_task)
    panic("Failed to load init ELF");
  serial_write_hex((ulong)term_task);
  serial_write_line("Tasks created!");
  scheduler_add(term_task);

  serial_write_line("Tasks loaded!");
  // scheduler_add(user_task);

  rtc_time t;
  rtc_read(&t);
  serial_printf("%04u-%02u-%02u %02u:%02u:%02u\n",
                t.year, t.month, t.day, t.hour, t.minute, t.second);

  pci_scan();

  // e1000 is at bus=0, dev=3, func=0
  pci_enable_bus_master(0, 3, 0);
  ulong e1000_mmio = pci_get_bar0(0, 3, 0);
  serial_write("e1000 MMIO base: ");
  serial_write_hex(e1000_mmio);
  serial_write_char('\n');
  e1000_init(e1000_mmio);

  dhcp_send_discovery();

  serial_write("Sleep...");
  timer_init(47);

  ksleep_ms(4000);
  serial_write_line("Done!");

  dns_resolve("www.google.com");

  icmp_send_ping(IP(10, 0, 2, 2));
  icmp_send_ping(IP(8, 8, 8, 8));

  // http_test();

  time_init();
  scheduler_run();

  serial_write("Goodbye from kernel!");
  for (;;)
    __asm__ volatile("hlt");
}