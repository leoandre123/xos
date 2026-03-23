#include "vmm.h"
#include "panic.h"
#include "pmm.h"
#include <stdint.h>

#define MAX_ADDRESS_SPACES 32

#define BOOT_CODE __attribute__((section(".bootstrap.text")))
#define BOOT_DATA __attribute__((section(".bootstrap.bss"), aligned(4096)))

static address_space_t g_kernel_space;
static address_space_t *g_current_space = 0;

static address_space_t g_space_pool[MAX_ADDRESS_SPACES];
static uint8_t g_space_used[MAX_ADDRESS_SPACES];

/* ---------------------------------------------------------
   Bootstrap-only page tables.
   These let the low bootstrap map the real kernel high.
   --------------------------------------------------------- */
BOOT_DATA static page_table_t g_boot_pml4;
BOOT_DATA static page_table_t g_boot_low_pdpt;
BOOT_DATA static page_table_t g_boot_low_pd;

BOOT_DATA static page_table_t g_boot_high_pdpt;
BOOT_DATA static page_table_t g_boot_high_pd;
BOOT_DATA static page_table_t g_boot_high_pt;

/* ---------------------------------------------------------
   Helpers
   --------------------------------------------------------- */

static inline uint64_t pml4_index(uint64_t addr) {
  return (addr >> 39) & 0x1FF;
}

static inline uint64_t pdpt_index(uint64_t addr) {
  return (addr >> 30) & 0x1FF;
}

static inline uint64_t pd_index(uint64_t addr) { return (addr >> 21) & 0x1FF; }

static inline uint64_t pt_index(uint64_t addr) { return (addr >> 12) & 0x1FF; }

static void memset64(uint64_t *ptr, uint64_t value, uint64_t count) {
  for (uint64_t i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static inline void write_cr3(uint64_t value) {
  asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

static inline uint64_t read_cr3(void) {
  uint64_t value;
  asm volatile("mov %%cr3, %0" : "=r"(value));
  return value;
}

static inline void invlpg(uint64_t virt) {
  asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* ---------------------------------------------------------
   Bootstrap VMM
   --------------------------------------------------------- */

BOOT_CODE static inline uint64_t boot_pml4_index(uint64_t addr) {
  return (addr >> 39) & 0x1FF;
}

BOOT_CODE static inline uint64_t boot_pdpt_index(uint64_t addr) {
  return (addr >> 30) & 0x1FF;
}

BOOT_CODE static inline uint64_t boot_pd_index(uint64_t addr) {
  return (addr >> 21) & 0x1FF;
}

BOOT_CODE static inline uint64_t boot_pt_index(uint64_t addr) {
  return (addr >> 12) & 0x1FF;
}

BOOT_CODE static void boot_memset64(uint64_t *ptr, uint64_t value,
                                    uint64_t count) {
  for (uint64_t i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

BOOT_CODE static inline void boot_write_cr3(uint64_t value) {
  asm volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

BOOT_CODE static void boot_zero_tables(void) {
  boot_memset64((uint64_t *)g_boot_pml4, 0, 512);
  boot_memset64((uint64_t *)g_boot_low_pdpt, 0, 512);
  boot_memset64((uint64_t *)g_boot_low_pd, 0, 512);
  boot_memset64((uint64_t *)g_boot_high_pdpt, 0, 512);
  boot_memset64((uint64_t *)g_boot_high_pd, 0, 512);
  boot_memset64((uint64_t *)g_boot_high_pt, 0, 512);
}

BOOT_CODE void vmm_bootstrap_init(void) {
  boot_zero_tables();

  /* Low identity map: first 1 GiB using 2 MiB pages */
  g_boot_pml4[0] = ((uint64_t)g_boot_low_pdpt & PAGE_ADDR_MASK) | PAGE_PRESENT |
                   PAGE_WRITABLE;

  g_boot_low_pdpt[0] =
      ((uint64_t)g_boot_low_pd & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;

  for (uint64_t i = 0; i < 512; i++) {
    g_boot_low_pd[i] =
        (i * PAGE_SIZE_2M) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
  }

  /* High-half kernel map: use 4 KiB pages */
  uint64_t kphys_start = ((uint64_t)__kernel_phys_start) & ~0xFFFULL;
  uint64_t kphys_end = (((uint64_t)__kernel_phys_end) + 0xFFFULL) & ~0xFFFULL;
  uint64_t kvirt_start = (uint64_t)__kernel_virt_start;

  uint64_t pml4i = boot_pml4_index(kvirt_start);
  uint64_t pdpti = boot_pdpt_index(kvirt_start);
  uint64_t pdi = boot_pd_index(kvirt_start);

  g_boot_pml4[pml4i] = ((uint64_t)g_boot_high_pdpt & PAGE_ADDR_MASK) |
                       PAGE_PRESENT | PAGE_WRITABLE;

  g_boot_high_pdpt[pdpti] = ((uint64_t)g_boot_high_pd & PAGE_ADDR_MASK) |
                            PAGE_PRESENT | PAGE_WRITABLE;

  g_boot_high_pd[pdi] = ((uint64_t)g_boot_high_pt & PAGE_ADDR_MASK) |
                        PAGE_PRESENT | PAGE_WRITABLE;

  uint64_t page_count = (kphys_end - kphys_start) / PAGE_SIZE;

  for (uint64_t i = 0; i < page_count; i++) {
    g_boot_high_pt[i] =
        (kphys_start + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;
  }

  boot_write_cr3((uint64_t)g_boot_pml4);
}

/* ---------------------------------------------------------
   Runtime VMM
   --------------------------------------------------------- */

static page_table_t *alloc_table(void) {
  uint64_t phys = pmm_alloc_page();
  if (!phys) {
    return 0;
  }

  page_table_t *table = (page_table_t *)phys; /* relies on low identity map */
  memset64((uint64_t *)table, 0, 512);
  return table;
}

static page_table_t *get_or_create_next(page_table_t *table, uint64_t index,
                                        uint64_t flags) {
  uint64_t entry = (*table)[index];

  if (entry & PAGE_PRESENT) {
    return (page_table_t *)(entry & PAGE_ADDR_MASK);
  }

  page_table_t *next = alloc_table();
  if (!next) {
    return 0;
  }

  (*table)[index] = ((uint64_t)next & PAGE_ADDR_MASK) | flags | PAGE_PRESENT;
  return next;
}

static void init_address_space(address_space_t *space) {
  space->pml4 = alloc_table();
  if (!space->pml4) {
    panic("VMM: failed to allocate PML4");
  }

  space->cr3 = (uint64_t)space->pml4;
}

int vmm_map_page(address_space_t *space, uint64_t virt, uint64_t phys,
                 uint64_t flags) {
  if (!space || !space->pml4) {
    return 0;
  }

  if ((virt & 0xFFFULL) != 0 || (phys & 0xFFFULL) != 0) {
    return 0;
  }

  uint64_t table_flags = PAGE_WRITABLE;
  if (flags & PAGE_USER) {
    table_flags |= PAGE_USER;
  }

  page_table_t *pdpt =
      get_or_create_next(space->pml4, pml4_index(virt), table_flags);
  if (!pdpt)
    return 0;

  page_table_t *pd = get_or_create_next(pdpt, pdpt_index(virt), table_flags);
  if (!pd)
    return 0;

  page_table_t *pt = get_or_create_next(pd, pd_index(virt), table_flags);
  if (!pt)
    return 0;

  uint64_t idx = pt_index(virt);
  if ((*pt)[idx] & PAGE_PRESENT) {
    return 0;
  }

  (*pt)[idx] = (phys & PAGE_ADDR_MASK) | flags | PAGE_PRESENT;
  return 1;
}

static int vmm_map_2m(address_space_t *space, uint64_t virt, uint64_t phys,
                      uint64_t flags) {
  if (!space || !space->pml4) {
    return 0;
  }

  if ((virt & (PAGE_SIZE_2M - 1)) != 0 || (phys & (PAGE_SIZE_2M - 1)) != 0) {
    return 0;
  }

  page_table_t *pdpt =
      get_or_create_next(space->pml4, pml4_index(virt), PAGE_WRITABLE);
  if (!pdpt)
    return 0;

  page_table_t *pd = get_or_create_next(pdpt, pdpt_index(virt), PAGE_WRITABLE);
  if (!pd)
    return 0;

  uint64_t idx = pd_index(virt);
  if ((*pd)[idx] & PAGE_PRESENT) {
    return 0;
  }

  (*pd)[idx] = (phys & PAGE_ADDR_MASK) | flags | PAGE_PRESENT | PAGE_HUGE;
  return 1;
}

int vmm_map_range(address_space_t *space, uint64_t virt, uint64_t phys,
                  uint64_t size, uint64_t flags) {
  uint64_t size_aligned = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

  for (uint64_t off = 0; off < size_aligned; off += PAGE_SIZE) {
    if (!vmm_map_page(space, virt + off, phys + off, flags)) {
      return 0;
    }
  }

  return 1;
}

int vmm_identity_map_range_2m(address_space_t *space, uint64_t start,
                              uint64_t end, uint64_t flags) {
  start &= ~(PAGE_SIZE_2M - 1);
  end = (end + PAGE_SIZE_2M - 1) & ~(PAGE_SIZE_2M - 1);

  for (uint64_t addr = start; addr < end; addr += PAGE_SIZE_2M) {
    if (!vmm_map_2m(space, addr, addr, flags)) {
      return 0;
    }
  }

  return 1;
}

uint64_t vmm_get_phys(address_space_t *space, uint64_t virt) {
  if (!space || !space->pml4) {
    return 0;
  }

  page_table_t *pml4 = space->pml4;
  uint64_t e = (*pml4)[pml4_index(virt)];
  if (!(e & PAGE_PRESENT))
    return 0;

  page_table_t *pdpt = (page_table_t *)(e & PAGE_ADDR_MASK);
  e = (*pdpt)[pdpt_index(virt)];
  if (!(e & PAGE_PRESENT))
    return 0;

  if (e & PAGE_HUGE) {
    return (e & PAGE_ADDR_MASK) + (virt & 0x3FFFFFFFULL);
  }

  page_table_t *pd = (page_table_t *)(e & PAGE_ADDR_MASK);
  e = (*pd)[pd_index(virt)];
  if (!(e & PAGE_PRESENT))
    return 0;

  if (e & PAGE_HUGE) {
    return (e & PAGE_ADDR_MASK) + (virt & 0x1FFFFFULL);
  }

  page_table_t *pt = (page_table_t *)(e & PAGE_ADDR_MASK);
  e = (*pt)[pt_index(virt)];
  if (!(e & PAGE_PRESENT))
    return 0;

  return (e & PAGE_ADDR_MASK) + (virt & 0xFFFULL);
}

int vmm_unmap_page(address_space_t *space, uint64_t virt) {
  if (!space || !space->pml4) {
    return 0;
  }

  uint64_t e = (*space->pml4)[pml4_index(virt)];
  if (!(e & PAGE_PRESENT))
    return 0;
  page_table_t *pdpt = (page_table_t *)(e & PAGE_ADDR_MASK);

  e = (*pdpt)[pdpt_index(virt)];
  if (!(e & PAGE_PRESENT))
    return 0;
  page_table_t *pd = (page_table_t *)(e & PAGE_ADDR_MASK);

  e = (*pd)[pd_index(virt)];
  if (!(e & PAGE_PRESENT) || (e & PAGE_HUGE))
    return 0;
  page_table_t *pt = (page_table_t *)(e & PAGE_ADDR_MASK);

  uint64_t idx = pt_index(virt);
  if (!((*pt)[idx] & PAGE_PRESENT))
    return 0;

  (*pt)[idx] = 0;
  invlpg(virt);
  return 1;
}

static void mount_address_space(address_space_t *space) {
  if (!space || !space->pml4) {
    panic("VMM: mount invalid address space");
  }

  write_cr3((uint64_t)space->pml4);
  g_current_space = space;
}

void vmm_switch_address_space(address_space_t *space) {
  mount_address_space(space);
}

address_space_t *vmm_kernel_space(void) { return &g_kernel_space; }

address_space_t *vmm_current_space(void) { return g_current_space; }

void vmm_init_runtime(void) {
  init_address_space(&g_kernel_space);

  /* Keep low identity map for now: first 4 GiB */
  if (!vmm_identity_map_range_2m(&g_kernel_space, 0, 0x100000000ULL,
                                 PAGE_WRITABLE)) {
    panic("VMM: low identity map failed");
  }

  /* Map the real kernel high */
  uint64_t kvirt = (uint64_t)__kernel_virt_start;
  uint64_t kphys = (uint64_t)__kernel_phys_start;
  uint64_t ksize = (uint64_t)__kernel_phys_end - (uint64_t)__kernel_phys_start;

  if (!vmm_map_range(&g_kernel_space, kvirt, kphys, ksize, PAGE_WRITABLE)) {
    panic("VMM: high kernel map failed");
  }

  mount_address_space(&g_kernel_space);
}

address_space_t *vmm_create_address_space(void) {
  for (int i = 0; i < MAX_ADDRESS_SPACES; i++) {
    if (!g_space_used[i]) {
      g_space_used[i] = 1;
      address_space_t *space = &g_space_pool[i];
      init_address_space(space);

      /* Copy kernel half: entries 256..511 */
      for (int j = 256; j < 512; j++) {
        (*space->pml4)[j] = (*g_kernel_space.pml4)[j];
      }

      return space;
    }
  }

  return 0;
}

static void destroy_table_user_half(page_table_t *table, int level) {
  for (int i = 0; i < 512; i++) {
    uint64_t entry = (*table)[i];
    if (!(entry & PAGE_PRESENT)) {
      continue;
    }

    uint64_t phys = entry & PAGE_ADDR_MASK;

    if (level == 1) {
      /* PTE leaf -> free mapped 4 KiB page */
      pmm_free_page(phys);
      continue;
    }

    if (entry & PAGE_HUGE) {
      /*
       * We do not create huge user mappings in this design.
       * If you add them later, free the underlying 2 MiB region here.
       */
      panic("VMM: huge user page in destroy");
    }

    destroy_table_user_half((page_table_t *)phys, level - 1);
    pmm_free_page(phys);
  }
}

void vmm_destroy_address_space(address_space_t *space) {
  if (!space || space == &g_kernel_space || !space->pml4) {
    return;
  }

  /* Only destroy user half. Kernel half is shared. */
  for (int i = 0; i < 256; i++) {
    uint64_t entry = (*space->pml4)[i];
    if (!(entry & PAGE_PRESENT)) {
      continue;
    }

    uint64_t phys = entry & PAGE_ADDR_MASK;
    destroy_table_user_half((page_table_t *)phys, 3);
    pmm_free_page(phys);
  }

  pmm_free_page((uint64_t)space->pml4);
  space->pml4 = 0;
  space->cr3 = 0;

  for (int i = 0; i < MAX_ADDRESS_SPACES; i++) {
    if (&g_space_pool[i] == space) {
      g_space_used[i] = 0;
      break;
    }
  }
}