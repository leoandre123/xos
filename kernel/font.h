#pragma once
#include <stdint.h>

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126
#define FONT_GLYPH_WIDTH 8
#define FONT_GLYPH_HEIGHT 16
#define FONT_GLYPH_COUNT 95

extern const uint8_t kernel_font[FONT_GLYPH_COUNT][FONT_GLYPH_HEIGHT];
