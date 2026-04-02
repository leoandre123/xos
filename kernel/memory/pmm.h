#pragma once
#include "boot_info.h"
#include "types.h"

void pmm_init(BootInfo *boot_info);
void pmm_remap_bitmap(ulong hhdm_base); // call after vmm_init sets up HHDM
ulong pmm_alloc_page();
void pmm_free_page(ulong addr);

ulong pmm_alloc_pages(uint count);
void pmm_free_pages(ulong addr, uint count);

ulong pmm_get_max_address();