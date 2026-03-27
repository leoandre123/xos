#include "../shared/boot_info.h"
#include "console.h"
#include "exceptions.h"
#include "gdt.h"
#include "graphics/gfx.h"
#include "idt.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "io/pic.h"
#include "io/timer.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "scheduler/scheduler.h"
#include "serial.h"
#include "test.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

void isr13_handler(uint64_t error_code) {
  console_write("EXCEPTION: General Protection Fault, error = 0x");
  console_write_hex64(error_code);
  console_write("\n");

  for (;;) {
    asm volatile("hlt");
  }
}

void isr14_handler(uint64_t error_code) {
  uint64_t cr2;
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
static void draw_rect(volatile uint32_t *fb, uint32_t pitch_pixels, uint32_t x,
                      uint32_t y, uint32_t w, uint32_t h, uint32_t color) {
  for (uint32_t yy = y; yy < y + h; yy++) {
    for (uint32_t xx = x; xx < x + w; xx++) {
      put_pixel(fb, pitch_pixels, xx, yy, color);
    }
  }
}

void test_pmm_alloc_many(void) {
  uint64_t pages[256];

  for (int i = 0; i < 256; i++) {
    pages[i] = pmm_alloc_page();
    ASSERT(pages[i] != 0);
    ASSERT(((uint64_t)pages[i] & 0xFFF) == 0);

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
static void draw_menu_item(int index, const char *text, int selection) {
  console_write(selection == index ? "> " : "  ");
  console_write(text);
  console_write("\n");
}

static void menu() {

  uint32_t bg = 0x00101050;
  int selection = 0;

  for (;;) {
    console_clear(bg);
    console_write_line("XOS - Kernel");

    draw_menu_item(0, "Test Heap", selection);
    draw_menu_item(1, "Run Memory Test", selection);
    draw_menu_item(2, "0/0", selection);
    draw_menu_item(3, "Power down", selection);

    KeyEvent ev;
    while ((ev = keyboard_last()).code == KEY_NONE) {
      asm volatile("hlt");
    }

    if (ev.code == KEY_UP && selection > 0)
      selection--;
    if (ev.code == KEY_DOWN && selection < 2)
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
        break;
      }
      selection = 0;
    }
    schedule();
  }
}

void task_2() {
  int x = 0;
  while (1) {
    serial_write("Task 2: ");
    serial_write_ulong(x++);
    serial_write_char('\n');
    schedule();
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
  idt_init();

  pic_remap(32, 40);
  pic_mask_all();
  pic_unmask_irq(0);
  pic_unmask_irq(1);

  exceptions_init();
  keyboard_init();
  timer_init();

  //
  serial_write("ENABLING STI...");
  asm volatile("sti");
  serial_write_line("DONE!");
  asm volatile("int $32");
  asm volatile("int $33");

  scheduler_init();
  task *menu_task = task_create_kernel(menu, 0, "KERNEL_MENU");
  task *task_ = task_create_kernel(task_2, 0, "TASK_2");
  // task *user_task = task_create_user(user_hello, 0, "USER_TASK");
  scheduler_add(menu_task);
  scheduler_add(task_);
  // scheduler_add(user_task);
  scheduler_run();

  serial_write("Goodbye from kernel!");
  for (;;)
    __asm__ volatile("hlt");
}