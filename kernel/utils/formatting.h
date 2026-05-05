#pragma once

#include <stdarg.h>
void emit_formatted_str(void (*emit)(char, void *), void *ctx, const char *fmt, va_list args);
