#ifndef CONSOLE_H
#define CONSOLE_H

#include "../shared/boot_info.h"
#include <stdint.h>

void console_init(BootInfo *boot_info);
void console_clear(uint32_t color);
void console_putc(char c);
void console_write(const char *s);
void console_write_line(const char *s);
void console_write_hex64(uint64_t value);
void console_write_u32(uint32_t value);
void console_set_fg_color(uint32_t value);
void console_set_bg_color(uint32_t value);

#endif