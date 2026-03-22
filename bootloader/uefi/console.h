#pragma once

#include <efi.h>
#include <efilib.h>

void console_clear();
void console_set_cursor(UINTN col, UINTN row);
void console_set_color(UINTN color);
void console_show_cursor(BOOLEAN visible);
void console_write(const CHAR16 *text);
void console_write_pos(const CHAR16 *text, int x, int y);
void console_write_line(const CHAR16 *text);
EFI_INPUT_KEY wait_for_key();

void console_draw_box(int x, int y, int width, int height);
