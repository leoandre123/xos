#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../shared/boot_info.h"
#include "console.h"
#include "idt.h"
#include "gdt.h"
#include "memory.h"
#include "serial.h"

static void put_pixel(volatile uint32_t *fb, uint32_t pitch_pixels,
                      uint32_t x, uint32_t y, uint32_t color)
{
    fb[y * pitch_pixels + x] = color;
}

static void fill_screen(volatile uint32_t *fb, uint32_t width,
                        uint32_t height, uint32_t pitch_pixels,
                        uint32_t color)
{
    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            put_pixel(fb, pitch_pixels, x, y, color);
        }
    }
}

void isr0_handler(void)
{
    console_write("EXCEPTION: Divide by zero\n");
    for (;;)
    {
        asm volatile("hlt");
    }
}

void isr3_handler(void)
{
    console_write("EXCEPTION: Breakpoint\n");
}

void isr13_handler(uint64_t error_code)
{
    console_write("EXCEPTION: General Protection Fault, error = 0x");
    console_write_hex64(error_code);
    console_write("\n");

    for (;;)
    {
        asm volatile("hlt");
    }
}

void isr14_handler(uint64_t error_code)
{
    uint64_t cr2;
    asm volatile("mov %%cr2, %0" : "=r"(cr2));

    console_write("EXCEPTION: Page Fault, error = 0x");
    console_write_hex64(error_code);
    console_write(", cr2 = 0x");
    console_write_hex64(cr2);
    console_write("\n");

    for (;;)
    {
        asm volatile("hlt");
    }
}

static void draw_rect(volatile uint32_t *fb, uint32_t pitch_pixels,
                      uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h,
                      uint32_t color)
{
    for (uint32_t yy = y; yy < y + h; yy++)
    {
        for (uint32_t xx = x; xx < x + w; xx++)
        {
            put_pixel(fb, pitch_pixels, xx, yy, color);
        }
    }
}

void kernel_main(BootInfo *boot_info)
{
    console_init(boot_info);
    console_clear(0x00101010);

    volatile uint32_t *fb = (volatile uint32_t *)(uint64_t)boot_info->framebuffer_base;
    uint32_t width = boot_info->framebuffer_width;
    uint32_t height = boot_info->framebuffer_height;
    uint32_t pitch_pixels = boot_info->framebuffer_pitch / 4;

    fill_screen(fb, width, height, pitch_pixels, 0x00202020);
    draw_rect(fb, pitch_pixels, 100, 100, 300, 150, 0x0000FF00);
    draw_rect(fb, pitch_pixels, 150, 150, 100, 100, 0x00FF0000);

    console_write("XOS KERNEL STARTED\n");
    console_putc('\n');
    console_putc('r');
    console_putc('\n');
    console_write("Framebuffer: ");
    console_write_u32(boot_info->framebuffer_width);
    console_putc('x');
    console_write_u32(boot_info->framebuffer_height);
    console_write(" pitch=");
    console_write_u32(boot_info->framebuffer_pitch);
    console_putc('\n');

    serial_init();
    serial_write("Hello from kernel\n");

    serial_write("Framebuffer OK\n");

    uint64_t test = 0x1122334455667788;
    console_write_hex64(test);

    gdt_init();
    uint16_t loc = idt_init();
    console_write("IDT loaded\nLocation: ");
    console_write_hex64(loc);

    asm volatile("int3");

    console_write("Back from int3\n");
    console_write_hex64(test);

    memory_map_print(boot_info);

    for (;;)
        __asm__ volatile("hlt");
}