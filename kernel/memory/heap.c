#include "heap.h"
#include "io/serial.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "types.h"
#include "utils/assert.h"
#include "utils/math.h"

#define HEAP_BLOCK_MIN_SIZE   align_up(sizeof(heap_block) + 1)
#define HEAP_BLOCK_END(block) (((ubyte *)block) + sizeof(heap_block) + ((block)->payload_size))

static heap g_heap;

static ulong align_up(ulong size) {
  return (size + HEAP_ALIGN_SIZE - 1) / HEAP_ALIGN_SIZE * HEAP_ALIGN_SIZE;
}

void heap_init() {
  serial_write_line("Heap 1");
  ulong phys_addr = pmm_alloc_pages(HEAP_INITAL_PAGES);
  serial_write_line("Heap 2");
  serial_write_line("Heap 3");
  vmm_map_pages(&g_kernel_address_space, KERNEL_HEAP, phys_addr,
                HEAP_INITAL_PAGES, PAGE_WRITABLE);
  serial_write_line("Heap 4");
  ulong heap_start = KERNEL_HEAP;
  ulong heap_end = KERNEL_HEAP + PAGE_SIZE * HEAP_INITAL_PAGES;
  g_heap.start = heap_start;
  g_heap.end = heap_end;
  g_heap.first_block = (heap_block *)heap_start;
  g_heap.first_block->payload_size = heap_end - heap_start - sizeof(heap_block);
  g_heap.first_block->free = 1;
  g_heap.first_block->next = 0;
  g_heap.first_block->prev = 0;
}

static void split_block(heap_block *block, ulong payload_size) {
  // Payload size must be aligned here
  ulong old_size = block->payload_size;
  block->payload_size = payload_size;
  heap_block *next_block = (heap_block *)HEAP_BLOCK_END(block);
  next_block->next = block->next;
  if (next_block->next) {
    next_block->next->prev = next_block;
  }
  next_block->prev = block;
  next_block->free = 1;
  next_block->payload_size = old_size - payload_size - sizeof(heap_block);
  block->next = next_block;
}

static heap_block *find_last_block() {
  heap_block *block = g_heap.first_block;
  while (1) {
    if (!block->next)
      return block;
    block = block->next;
  }
}

static heap_block *expand_heap(ulong min_size) {
  serial_write_line("EXPANDING HEAP");
  heap_block *last_block = find_last_block();
  ASSERT(HEAP_BLOCK_END(last_block) == (ubyte *)g_heap.end);
  ulong min_pages = (min_size + PAGE_SIZE - 1) / PAGE_SIZE;
  ulong page_count = MAX(min_pages, HEAP_EXPAND_PAGES);

  ulong phys_addr = pmm_alloc_pages(page_count);

  vmm_map_pages(&g_kernel_address_space, g_heap.end, phys_addr,
                page_count, PAGE_WRITABLE);
  g_heap.end += page_count * PAGE_SIZE;

  if (last_block->free) {
    last_block->payload_size += page_count * PAGE_SIZE;
    return last_block;
  } else {
    heap_block *block = (heap_block *)HEAP_BLOCK_END(last_block);
    block->free = 1;
    block->next = 0;
    block->prev = last_block;
    block->payload_size = page_count * PAGE_SIZE - sizeof(heap_block);
    last_block->next = block;
    return block;
  }
}

static heap_block *find_free_block(ulong min_size) {
  heap_block *block = g_heap.first_block;
  while (block) {
    if (block->free && block->payload_size >= min_size) {
      return block;
    }
    block = block->next;
  }
  return 0;
}

void *kmalloc(ulong size) {
  if (size == 0) {
    return 0;
  }
  ulong aligned_size = align_up(size);

  heap_block *free_block = find_free_block(aligned_size);

  if (!free_block) {
    free_block = expand_heap(sizeof(heap_block) + aligned_size);
  }
  ASSERT(free_block);
  ASSERT(free_block->payload_size >= aligned_size);

  if (free_block->payload_size >= aligned_size + HEAP_BLOCK_MIN_SIZE) {
    split_block(free_block, aligned_size);
  }
  free_block->free = 0;
  return ((ubyte *)free_block) + sizeof(heap_block);
}
void kfree(void *addr) {
  heap_block *block = g_heap.first_block;
  while (block) {
    if (block == (heap_block *)addr) {
      block->free = 1;
      return;
    }
    block = block->next;
  }
}

heap *get_kernel_heap() { return &g_heap; }