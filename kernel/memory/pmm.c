#include "pmm.h"
#include "memory.h"
#include "panic.h"
#include <stdint.h>

typedef struct {
  uint64_t total_memory;
  uint64_t usable_memory;
  uint64_t reserved_memory;
  uint64_t used_memory;

  uint64_t page_count;

  uint8_t *bitmap;
  uint64_t bitmap_size;

  uint64_t last_index;
} pmm_state_t;

static pmm_state_t g_pmm;

static inline uint64_t addr_to_page(uint64_t addr) { return (addr) >> 12; }
static inline uint64_t page_to_addr(uint64_t page) { return page << 12; }

static void memset8(uint8_t *ptr, uint8_t value, uint64_t count) {
  for (uint64_t i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static inline void bitmap_set(uint64_t bit) {
  g_pmm.bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(uint64_t bit) {
  g_pmm.bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(uint64_t bit) {
  return (g_pmm.bitmap[bit / 8] >> (bit % 8)) & 1;
}

static void pmm_reserve_page(uint64_t addr) {
  uint64_t page = addr_to_page(addr);
  if (!bitmap_test(page)) {
    bitmap_set(page);
    g_pmm.used_memory += 4096;
  }
}
static void pmm_unreserve_page(uint64_t addr) {
  uint64_t page = addr_to_page(addr);
  if (bitmap_test(page)) {
    bitmap_clear(page);
    g_pmm.used_memory -= 4096;
  }
}

static void pmm_reserve_region(uint64_t addr, int size) {
  uint64_t start = (addr & ~0xFFFULL);
  uint64_t end = (addr + size + 0xFFFULL) & ~0xFFFULL;
  uint64_t pages = (end - start) / 4096;

  for (uint64_t i = 0; i < pages; i++)
    pmm_reserve_page(start + i * 4096);
}

void pmm_init(BootInfo *boot_info) {

  uint64_t page_count = 0;
  uint8_t *cur = (uint8_t *)(uint64_t)boot_info->memory_map;
  uint8_t *end = cur + boot_info->memory_map_size;

  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;
    page_count += desc->NumberOfPages;
    cur += boot_info->memory_map_desc_size;
  }

  g_pmm.bitmap = 0;
  g_pmm.page_count = page_count;
  g_pmm.bitmap_size = (page_count + 7) / 8;
  g_pmm.used_memory = g_pmm.page_count * 4096ULL;
  g_pmm.last_index = 0;

  cur = (uint8_t *)(uint64_t)boot_info->memory_map;
  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;

    if (desc->Type == 7 && desc->NumberOfPages * 4096ULL >= g_pmm.bitmap_size) {
      g_pmm.bitmap = (uint8_t *)desc->PhysicalStart;
      break;
    }

    cur += boot_info->memory_map_desc_size;
  }
  if (!g_pmm.bitmap) {
    panic("PMM: failed to find space for bitmap");
  }

  memset8(g_pmm.bitmap, 0xFF, g_pmm.bitmap_size);

  cur = (uint8_t *)(uint64_t)boot_info->memory_map;
  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;

    if (desc->Type == 7) {
      for (uint64_t i = 0; i < desc->NumberOfPages; i++) {
        pmm_unreserve_page((desc->PhysicalStart) + i * 4096ULL);
      }
    }
    cur += boot_info->memory_map_desc_size;
  }

  pmm_reserve_page(0);
  pmm_reserve_region((uint64_t)g_pmm.bitmap, g_pmm.bitmap_size);
}

uint64_t pmm_alloc_page() {
  for (uint64_t i = g_pmm.last_index; i < g_pmm.page_count; i++) {
    if (!bitmap_test(i)) {
      bitmap_set(i);
      g_pmm.used_memory += 4096;
      g_pmm.last_index = i + 1;
      return page_to_addr(i);
    }
  }

  for (uint64_t i = 0; i < g_pmm.last_index; i++) {
    if (!bitmap_test(i)) {
      bitmap_set(i);
      g_pmm.used_memory += 4096;
      g_pmm.last_index = i + 1;
      return page_to_addr(i);
    }
  }
  panic("Out of memory!");
  return 0;
}

void pmm_free_page(uint64_t addr) {
  uint64_t page = addr_to_page(addr);

  if (((uint64_t)addr & 0xFFFULL) != 0) {
    panic("PMM free unaligned address");
  }
  if (page >= g_pmm.page_count) {
    panic("PMM free out of range");
  }
  if (!bitmap_test(page)) {
    panic("PMM double free or freeing free page");
  }

  bitmap_clear(page);
  g_pmm.used_memory -= 4096;

  if (page < g_pmm.last_index) {
    g_pmm.last_index = page;
  }
}