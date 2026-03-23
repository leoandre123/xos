#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include "types.h"

typedef struct BootInfo {
  ulong framebuffer_base;
  uint framebuffer_width;
  uint framebuffer_height;
  uint framebuffer_pitch;
  ulong memory_map;
  ulong memory_map_size;
  ulong memory_map_desc_size;
  uint memory_map_desc_version;
} BootInfo;

#endif