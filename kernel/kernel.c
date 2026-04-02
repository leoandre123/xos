#include "../shared/boot_info.h"
#include "cpu/exceptions.h"
#include "cpu/gdt.h"
#include "cpu/idt.h"
#include "cpu/syscall.h"
#include "filesystem/elf.h"
#include "filesystem/fat32.h"
#include "graphics/console.h"
#include "graphics/gfx.h"
#include "io/ata.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "io/pic.h"
#include "io/serial.h"
#include "io/timer.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "scheduler/scheduler.h"
#include <stdbool.h>
#include <stddef.h>

uint g_fb_width;
uint g_fb_height;
uint g_fb_pitch;
ulong g_fb_phys;

extern void enter_kernel_main(ulong stack_addr);

void panic(const char *msg) {
  serial_write("KERNEL PANIC: ");
  serial_write(msg);

  // Stop everything
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

void panic_assert(const char *file, int line, const char *expr) {

  console_write("ASSERT FAILED: ");
  console_write_line(expr);

  console_write("FILE: ");
  console_write_line(file);

  console_write("LINE: ");
  console_write_u32(line);
  console_write("\n");

  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

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

// FE

// DM
// DAFNE
void kernel_main() {
  serial_write_line("LeOS!");
  serial_write_line("Hello from kernel!");

  ulong rsp;
  asm volatile("mov %%rsp, %0" : "=r"(rsp));

  serial_write("RSP: ");
  serial_write_hex(rsp);
  serial_write_char('\n');

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

  exceptions_init();
  keyboard_init();

  ata_init();
  fat32_init(0);

  fat32_print_root();

  fat32_file *f0 = fat32_open("/a_very_long_filename_is_here_right_here.txt");
  fat32_file *f1 = fat32_open("/hello.txt");
  fat32_file *f2 = fat32_open("/a.b.txt");
  serial_write("BEFORE PF");
  fat32_file *f3 = fat32_open("/subfolder/hello.txt");
  serial_write("AFTER PF");
  ubyte *file_buf0 = kmalloc(f0->size);
  ubyte *file_buf1 = kmalloc(f1->size);
  ubyte *file_buf2 = kmalloc(f2->size);
  ubyte *file_buf3 = kmalloc(f3->size);
  fat32_read(f0, file_buf0, f0->size);
  fat32_read(f1, file_buf1, f1->size);
  fat32_read(f2, file_buf2, f2->size);
  fat32_read(f3, file_buf3, f3->size);

  serial_write(file_buf0);
  serial_write(file_buf1);
  serial_write(file_buf2);
  serial_write(file_buf3);

  // fat32_file files[] = fat32_get_files();

  //

  serial_write("ENABLING STI...");
  asm volatile("sti");
  // asm volatile("cli");
  serial_write_line("DONE!");
  asm volatile("int $32");
  asm volatile("int $33");

  // asm volatile("int $3");

  scheduler_init();
  serial_write_line("Scheduler initilized!");

  fat32_file *term_file = fat32_open("/terminal.elf");
  if (!term_file)
    panic("Cannot find /terminal.elf on disk");
  task *term_task = elf_load(term_file, "terminal", "/");
  if (!term_task)
    panic("Failed to load terminal ELF");
  serial_write_hex((ulong)term_task);
  serial_write_line("Tasks created!");
  scheduler_add(term_task);

  serial_write_line("Tasks loaded!");
  // scheduler_add(user_task);

  timer_init(47);
  scheduler_run();

  serial_write("Goodbye from kernel!");
  for (;;)
    __asm__ volatile("hlt");
}