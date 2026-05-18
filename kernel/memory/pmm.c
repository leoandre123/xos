#include "pmm.h"
#include "graphics/console.h"
#include "io/serial.h"
#include "memory.h"
#include "panic.h"
#include "types.h"

typedef struct {
  ulong total_memory;
  ulong usable_memory;
  ulong reserved_memory;
  ulong used_memory;

  ulong page_count;

  ubyte *bitmap;
  ulong bitmap_size;

  ulong last_index;
  ulong max_address;
} pmm_state_t;

extern char __kernel_phys_start[];
extern char __kernel_phys_end[];
extern char __kernel_virt_start[];
extern char __kernel_virt_end[];

static pmm_state_t g_pmm;

static inline ulong addr_to_page(ulong addr) { return (addr) >> 12; }
static inline ulong page_to_addr(ulong page) { return page << 12; }

static void memset8(ubyte *ptr, ubyte value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static inline void bitmap_set(ulong bit) {
  g_pmm.bitmap[bit / 8] |= (1 << (bit % 8));
}

static inline void bitmap_clear(ulong bit) {
  g_pmm.bitmap[bit / 8] &= ~(1 << (bit % 8));
}

static inline int bitmap_test(ulong bit) {
  return (g_pmm.bitmap[bit / 8] >> (bit % 8)) & 1;
}

static void pmm_reserve_page(ulong addr) {
  ulong page = addr_to_page(addr);
  if (!bitmap_test(page)) {
    bitmap_set(page);
    g_pmm.used_memory += 4096;
  }
}
static void pmm_unreserve_page(ulong addr) {
  ulong page = addr_to_page(addr);
  if (bitmap_test(page)) {
    bitmap_clear(page);
    g_pmm.used_memory -= 4096;
  }
}

static void pmm_reserve_region(ulong addr, int size) {
  ulong start = (addr & ~0xFFFULL);
  ulong end = (addr + size + 0xFFFULL) & ~0xFFFULL;
  ulong pages = (end - start) / 4096;

  for (ulong i = 0; i < pages; i++)
    pmm_reserve_page(start + i * 4096);
}

void pmm_init(BootInfo *boot_info) {

  ubyte *cur = (ubyte *)(ulong)boot_info->memory_map;
  ubyte *end = cur + boot_info->memory_map_size;

  // Size the bitmap by the highest EfiConventionalMemory page index, not the
  // sum of all pages. Real UEFI maps include huge MMIO descriptors that would
  // inflate the sum and make the bitmap impossibly large.
  ulong max_page = 0;
  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;
    if (desc->Type == 7 && desc->NumberOfPages > 0) {
      ulong last = addr_to_page(desc->PhysicalStart + (desc->NumberOfPages - 1) * 4096ULL);
      if (last > max_page)
        max_page = last;
    }
    cur += boot_info->memory_map_desc_size;
  }
  ulong page_count = max_page + 1;

  g_pmm.bitmap = 0;
  g_pmm.page_count = page_count;
  g_pmm.bitmap_size = (page_count + 7) / 8;
  g_pmm.used_memory = g_pmm.page_count * 4096ULL;
  g_pmm.last_index = 0;
  g_pmm.max_address = 0;

  console_writef("PMM: desc_sz=%u map_sz=%u bitmap_sz=%u\n",
                 (uint)boot_info->memory_map_desc_size,
                 (uint)boot_info->memory_map_size,
                 (uint)g_pmm.bitmap_size);

  cur = (ubyte *)(ulong)boot_info->memory_map;
  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;
    if (desc->Type == 7)
      console_writef("  [FREE] phys=%x pages=%u\n",
                     desc->PhysicalStart, (uint)desc->NumberOfPages);
    cur += boot_info->memory_map_desc_size;
  }

  bool found = false;
  cur = (ubyte *)(ulong)boot_info->memory_map;
  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;

    if (desc->Type == 7 &&
        desc->PhysicalStart != 0 &&
        desc->NumberOfPages * 4096ULL >= g_pmm.bitmap_size) {
      g_pmm.bitmap = (ubyte *)desc->PhysicalStart;
      found = true;
      break;
    }

    cur += boot_info->memory_map_desc_size;
  }
  if (!found) {
    panic("PMM: failed to find space for bitmap");
  } else {
    console_writef("PMM Bitmap allocated");
  }

  memset8(g_pmm.bitmap, 0xFF, g_pmm.bitmap_size);

  cur = (ubyte *)(ulong)boot_info->memory_map;
  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;

    if (desc->Type == 7) {
      for (ulong i = 0; i < desc->NumberOfPages; i++) {
        ulong addr = (desc->PhysicalStart) + i * 4096ULL;
        pmm_unreserve_page(addr);
        g_pmm.max_address = addr;
        g_pmm.total_memory += 4096ULL;
      }
    }
    cur += boot_info->memory_map_desc_size;
  }

  pmm_reserve_page(0);
  pmm_reserve_region((ulong)g_pmm.bitmap, g_pmm.bitmap_size);
  pmm_reserve_region(((ulong)__kernel_phys_start),
                     ((ulong)__kernel_phys_end) - ((ulong)__kernel_phys_start));

  serial_write("Pages: ");
  serial_write_ulong(g_pmm.page_count);
  serial_write_char('\n');
}

void pmm_remap_bitmap(ulong hhdm_base) {
  g_pmm.bitmap = (ubyte *)((ulong)g_pmm.bitmap + hhdm_base);
}

ulong pmm_alloc_page() {
  for (ulong i = g_pmm.last_index; i < g_pmm.page_count; i++) {
    if (!bitmap_test(i)) {
      bitmap_set(i);
      g_pmm.used_memory += 4096;
      g_pmm.last_index = i + 1;
      return page_to_addr(i);
    }
  }

  for (ulong i = 0; i < g_pmm.last_index; i++) {
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

void pmm_free_page(ulong addr) {
  ulong page = addr_to_page(addr);

  if (((ulong)addr & 0xFFFULL) != 0) {
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

ulong pmm_alloc_pages(uint count) {
  uint current_count = 0;
  ulong first_page = 0;
  for (ulong i = g_pmm.last_index; i < g_pmm.page_count; i++) {
    if (bitmap_test(i)) {
      current_count = 0;
    } else {
      if (current_count == 0) {
        first_page = i;
      }
      current_count++;
      if (current_count == count) {
        for (uint i = 0; i < count; i++) {
          bitmap_set(first_page + i);
        }
        g_pmm.used_memory += count * 4096;
        g_pmm.last_index = first_page + count;
        return page_to_addr(first_page);
      }
    }
  }
  current_count = 0;
  for (ulong i = 0; i < g_pmm.last_index; i++) {
    if (bitmap_test(i)) {
      current_count = 0;
    } else {
      if (current_count == 0) {
        first_page = i;
      }
      current_count++;
      if (current_count == count) {
        for (uint i = 0; i < count; i++) {
          bitmap_set(first_page + i);
        }
        g_pmm.used_memory += count * 4096;
        g_pmm.last_index = first_page + count;
        return page_to_addr(first_page);
      }
    }
  }
  panic("Out of memory!");
  return 0;
}
void pmm_free_pages(ulong addr, uint count) {
  panic("pmm_free_pages not implemented!");
}

ulong pmm_get_max_address() {
  return g_pmm.max_address;
}

void pmm_get_stats(mem_info *info) {
  info->total = g_pmm.page_count * 4096ULL;
  info->used  = g_pmm.used_memory;
  info->free  = info->total - info->used;
}