#pragma once
#include <stddef.h>
#include <stdint.h>

#define KERNEL_PHYS_BASE 0x00200000ULL
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

#define PAGE_SIZE 0x1000ULL
#define PAGE_SIZE_2M 0x200000ULL

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_WRITABLE (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_HUGE (1ULL << 7)
#define PAGE_NX (1ULL << 63)

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

typedef uint64_t page_table_t[512];

typedef struct {
  page_table_t *pml4;
  uint64_t cr3;
} address_space_t;

/* Linker symbols */
extern char __bootstrap_phys_start[];
extern char __bootstrap_phys_end[];

extern char __kernel_phys_start[];
extern char __kernel_phys_end[];
extern char __kernel_virt_start[];
extern char __kernel_virt_end[];

/* Early bootstrap paging: callable before PMM init */
void vmm_bootstrap_init(void);

/* Full runtime VMM: call after PMM init */
void vmm_init_runtime(void);

address_space_t *vmm_kernel_space(void);
address_space_t *vmm_current_space(void);

void vmm_switch_address_space(address_space_t *space);

int vmm_map_page(address_space_t *space, uint64_t virt, uint64_t phys,
                 uint64_t flags);
int vmm_unmap_page(address_space_t *space, uint64_t virt);
uint64_t vmm_get_phys(address_space_t *space, uint64_t virt);

int vmm_map_range(address_space_t *space, uint64_t virt, uint64_t phys,
                  uint64_t size, uint64_t flags);
int vmm_identity_map_range_2m(address_space_t *space, uint64_t start,
                              uint64_t end, uint64_t flags);

address_space_t *vmm_create_address_space(void);
void vmm_destroy_address_space(address_space_t *space);