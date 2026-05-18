#pragma once
#include "cdefs.h"
#include "image.h"

EXTERN_C_BEGIN

// Load the .icon section from an ELF file as a bitmap.
// Returns null if the file has no embedded icon.
// Caller owns the returned allocation (sys_free).
bitmap *elf_icon_load(const char *path);

EXTERN_C_END
