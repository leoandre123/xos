#pragma once
#include "boot_info.h"

typedef struct
{
  uint Type;
  uint Pad;
  ulong PhysicalStart;
  ulong VirtualStart;
  ulong NumberOfPages;
  ulong Attribute;
} EfiMemoryDescriptor;

void memory_map_print(BootInfo *boot_info);
ulong memory_get_total_usable_bytes(BootInfo *boot_info);