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
#include "test.h"
#include <stdbool.h>
#include <stddef.h>

static uint fb_width;
static uint fb_height;
static uint fb_pitch;

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

void isr0_handler(void) {
  console_write("EXCESSSPTION: Divide by zero\n");
  for (;;) {
    asm volatile("hlt");
  }
}

void isr3_handler(void) { console_write("EXCEPTION: Breakpoint\n"); }

void isr13_handler(ulong error_code) {
  console_write("EXCEPTION: General Protection Fault, error = 0x");
  console_write_hex64(error_code);
  console_write("\n");

  for (;;) {
    asm volatile("hlt");
  }
}

void isr14_handler(ulong error_code) {
  ulong cr2;
  asm volatile("mov %%cr2, %0" : "=r"(cr2));

  console_write("EXCEPTION: Page Fault, error = 0x");
  console_write_hex64(error_code);
  console_write(", cr2 = 0x");
  console_write_hex64(cr2);
  console_write("\n");

  for (;;) {
    asm volatile("hlt");
  }
}
/*
static void draw_rect(volatile uint *fb, uint pitch_pixels, uint x,
                      uint y, uint w, uint h, uint color) {
  for (uint yy = y; yy < y + h; yy++) {
    for (uint xx = x; xx < x + w; xx++) {
      put_pixel(fb, pitch_pixels, xx, yy, color);
    }
  }
}

void test_pmm_alloc_many(void) {
  ulong pages[256];

  for (int i = 0; i < 256; i++) {
    pages[i] = pmm_alloc_page();
    ASSERT(pages[i] != 0);
    ASSERT(((ulong)pages[i] & 0xFFF) == 0);

    for (int j = 0; j < i; j++) {
      ASSERT(pages[i] != pages[j]);
    }
  }

  for (int i = 0; i < 256; i++) {
    pmm_free_page(pages[i]);
  }
}

void memory_test() {
  console_clear(0);
  console_set_fg_color(0x00ff0000);
  console_write("MEMORY TEST");

  test_pmm_alloc_many();

  keyboard_last();
  KeyEvent ev;
  while ((ev = keyboard_last()).code == KEY_NONE) {
    asm volatile("hlt");
  }

  console_set_fg_color(0x00ffffff);
}
*/

void drive_test() {
  serial_write_line("====DRIVE TEST====");
  char buf[512];
  ata_read(0, 1, &buf);
  serial_write(buf);
  for (int i = 0; i < 10; i++) {
    serial_write_hex(i);
    serial_write(": ");
    serial_write_hex(buf[i]);
    serial_write("   - ");
    serial_write_char(buf[i]);
    serial_write_char('\n');
  }
  serial_write(buf);
  serial_write_char('\n');
  serial_write_line("==================");
}

static void draw_menu_item(int index, const char *text, int selection) {
  console_write(selection == index ? "> " : "  ");
  console_write(text);
  console_write("\n");
}
/*
static void menu() {

  uint bg = 0x00101050;
  int selection = 0;

  for (;;) {
    console_clear(bg);
    console_write_line("XOS - Kernel");

    draw_menu_item(0, "Test Heap", selection);
    draw_menu_item(1, "Run Memory Test", selection);
    draw_menu_item(2, "0/0", selection);
    draw_menu_item(3, "Read from disk", selection);
    draw_menu_item(4, "Power down", selection);

    KeyEvent ev;
    while ((ev = keyboard_last()).code == KEY_NONE) {
      asm volatile("hlt");
    }

    if (ev.code == KEY_UP && selection > 0)
      selection--;
    if (ev.code == KEY_DOWN && selection < 4)
      selection++;
    if (ev.code == KEY_RETURN) {
      switch (selection) {
      case 0:
        test_heap();
        break;
      case 1:
        // memory_test();
        break;
      case 2: {
        uint y;
        y = 5 / (2 - selection);
        if (y) {
          serial_write("CRASH");
        }
        break;
      }
      case 3:
        drive_test();
        break;
      case 4:
        break;
      }
      selection = 0;
    }
  }
}
*/
void task_2() {
  int x = 0;
  while (1) {
    serial_write("Task 2: ");
    serial_write_ulong(x++);
    serial_write_char('\n');
    // schedule();
  }
}

static void user_hello(void *args) {
  // This runs in ring 3
  // Can't call kernel functions directly here
  // Just loop for now — a GPF here means ring 3 is working
  for (;;) {
  }
}

void kernel_pre_main(BootInfo *boot_info) {
  serial_init();
  fb_width = boot_info->framebuffer_width;
  fb_height = boot_info->framebuffer_height;
  fb_pitch = boot_info->framebuffer_pitch;

  pmm_init(boot_info);
  serial_write("PMM initilized!\n");
  vmm_init(boot_info);
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

  gfx_init((uint *)FB_BASE, fb_width, fb_height, fb_pitch);
  console_init(FB_BASE, fb_width, fb_height, fb_pitch);
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

  fat32_file *shell_file = fat32_open("/shell.elf");
  if (!shell_file)
    panic("Cannot find /shell.elf on disk");
  task *shell_task = elf_load(shell_file);
  if (!shell_task)
    panic("Failed to load shell ELF");
  serial_write_hex((ulong)shell_task);
  serial_write_line("Tasks created!");
  scheduler_add(shell_task);

  serial_write_line("Tasks loaded!");
  // scheduler_add(user_task);

  timer_init(47);
  scheduler_run();

  serial_write("Goodbye from kernel!");
  for (;;)
    __asm__ volatile("hlt");
}