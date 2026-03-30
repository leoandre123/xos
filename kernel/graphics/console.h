#pragma once
#include "types.h"

void console_init(ulong fb_base, uint width, uint height, uint pitch);
void console_clear(uint color);
void console_putc(char c);
void console_write(const char *s);
void console_write_line(const char *s);
void console_write_hex64(ulong value);
void console_write_u32(uint value);
void console_set_fg_color(uint value);
void console_set_bg_color(uint value);
