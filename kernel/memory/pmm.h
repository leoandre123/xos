#pragma once
#include "boot_info.h"
#include <stdint.h>

void pmm_init(BootInfo *boot_info);
uint64_t pmm_alloc_page();
void pmm_free_page();