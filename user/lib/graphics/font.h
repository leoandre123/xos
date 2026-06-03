#pragma once
#include "types.h"

typedef struct {

} font;

typedef struct {
} glyph;

typedef ulong glyph_id;

font *font_load(const char *path);
void font_free(font *font);

glyph_id font_get_glyph(font *f, uint unicode);