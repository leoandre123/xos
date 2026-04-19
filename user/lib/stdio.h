#pragma once

void printf(const char *fmt, ...);
int  sprintf(char *buf, const char *fmt, ...);
int  snprintf(char *buf, int size, const char *fmt, ...);
int  vsnprintf(char *buf, int size, const char *fmt, __builtin_va_list args);
void putchar(char c);
void puts(const char *s);
