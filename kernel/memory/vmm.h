#pragma once
#include "boot_info.h"
#include "types.h"

#define KERNEL_PHYS_BASE 0x00200000ULL

#define KERNEL_BASE       0xFFFFFFFF80000000ULL
#define HHDM_BASE         0xFFFF800000000000ULL
#define FB_BASE           0xFFFF900000000000ULL
#define KERNEL_HEAP       0xFFFFA00000000000ULL
#define KERNEL_STACK_BASE 0xFFFFB00000000000ULL
#define KERNEL_STACK_TOP  0xFFFFC00000000000ULL

#define KERNEL_STACK_PAGE_COUNT 0x400ULL

#define PAGE_SIZE    0x1000ULL
#define PAGE_SIZE_2M 0x200000ULL

#define PAGE_PRESENT  (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER     (1ULL << 2)
#define PAGE_HUGE     (1ULL << 7)
#define PAGE_NX       (1ULL << 63)

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

#define PHYS_TO_HHDM(p) ((void *)((ulong)(p) + HHDM_BASE))
#define HHDM_TO_PHYS(v) ((ulong)(v) - HHDM_BASE)

typedef struct {
  ulong entries[512];
} page_table;

typedef struct {
  ulong pml4_phys;
  page_table *pml4;
} address_space;

/* Linker symbols */
extern char __kernel_phys_start[];
extern char __kernel_phys_end[];
extern char __kernel_virt_start[];
extern char __kernel_virt_end[];

extern address_space g_kernel_address_space;

void vmm_init(BootInfo *boot_info);

address_space *vmm_create_address_space(void);
void vmm_destroy_address_space(address_space *space);
void vmm_switch_address_space(address_space *space);

ulong vmm_setup_stack();

void vmm_kernel_heap_alloc(ulong offset, ulong size);
void vmm_kernel_heap_free(ulong offset, ulong size);

// address_space *vmm_get_kernel_space();
void vmm_map_pages(address_space *space, ulong virtual_addr,
                   ulong phys_addr, ulong page_count, ulong flags);
void vmm_map_bytes(address_space *space, ulong virtual_addr,
                   ulong phys_addr, ulong size, ulong flags);