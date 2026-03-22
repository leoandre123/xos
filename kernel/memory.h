#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../shared/boot_info.h"

typedef struct
{
    uint32_t Type;
    uint32_t Pad;
    uint64_t PhysicalStart;
    uint64_t VirtualStart;
    uint64_t NumberOfPages;
    uint64_t Attribute;
} EfiMemoryDescriptor;

void memory_map_print(BootInfo *boot_info);
uint64_t memory_get_total_usable_bytes(BootInfo *boot_info);