#pragma once
#include "types.h"
void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *s);
void serial_write_line(const char *s);
void serial_write_ulong(ulong value);
void serial_write_hex(ulong value);