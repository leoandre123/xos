#include "../shared/boot_info.h"
// #include "assert.h"
// #include "console.h"
// #include "gdt.h"
// #include "idt.h"
// #include "io/keyboard.h"
// #include "io/keys.h"
// #include "io/pic.h"
// #include "io/timer.h"
// #include "memory/pmm.h"
#include "memory/vmm.h"
#include "serial.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void panic(const char *msg) {
  serial_write("KERNEL PANIC: ");
  serial_write(msg);

  // Stop everything
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
}

/*
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

static void put_pixel(volatile uint32_t *fb, uint32_t pitch_pixels, uint32_t x,
                      uint32_t y, uint32_t color) {
  fb[y * pitch_pixels + x] = color;
}

static void fill_screen(volatile uint32_t *fb, uint32_t width, uint32_t height,
                        uint32_t pitch_pixels, uint32_t color) {
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      put_pixel(fb, pitch_pixels, x, y, color);
    }
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

static void draw_menu_item(int index, const char *text, int selection) {
  console_write(selection == index ? "> " : "  ");
  console_write(text);
  console_write("\n");
}

static void menu() {

  uint32_t bg = 0x00101050;
  int selection = 0;

  int frame = 0;

  for (;;) {
    frame++;
    console_clear(bg);
    console_write("XOS - Kernel");
    console_write_u32(frame);
    console_write("\n");
    console_write_u32(g_timer_ticks);
    console_write("\n");
    console_write_line("ADDRESS: ");
    console_write_hex64((uint64_t)&frame);
    console_write("\n");

    draw_menu_item(0, "Test 1", selection);
    draw_menu_item(1, "Run Memory Test", selection);
    draw_menu_item(2, "Power down", selection);

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
        break;
      case 1:
        memory_test();
        break;
      case 2:
        break;
      }
      selection = 0;
    }
  }
}
*/

void kernel_main(BootInfo *boot_info) {
  asm volatile("mov $0x3F8, %%dx\n\t"
               "mov $'R', %%al\n\t"
               "out %%al, %%dx"
               :
               :
               : "ax", "dx");
  // pmm_init(boot_info);
  // vmm_init_runtime();
  serial_init();
  asm volatile("mov $0x3F8, %%dx\n\t"
               "mov $'N', %%al\n\t"
               "out %%al, %%dx"
               :
               :
               : "ax", "dx");
  const char msg[] = {'H', 'e', 'l', 'l', 'o', '\0'};
  serial_write(msg);
  serial_write("Hello from kernel\n");
  asm volatile("mov $0x3F8, %%dx\n\t"
               "mov $'E', %%al\n\t"
               "out %%al, %%dx"
               :
               :
               : "ax", "dx");
  for (;;)
    asm volatile("hlt");
  /*
  console_init(boot_info);

  gdt_init();
  idt_init();

  pic_remap(32, 40);
  pic_mask_all();
  pic_unmask_irq(0);
  pic_unmask_irq(1);

  keyboard_init();
  timer_init();

  asm volatile("sti");
  asm volatile("int $32");
  asm volatile("int $33");
  menu();

  for (;;)
   __asm__ volatile("hlt");
  */
}
