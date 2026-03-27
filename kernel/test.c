#include "test.h"
#include "console.h"
#include "graphics/gfx.h"
#include "io/keyboard.h"
#include "io/keys.h"
#include "memory/heap.h"
#include "memory/vmm.h"
#include "serial.h"

void test_heap() {
  heap *kernel_heap = get_kernel_heap();

  serial_write_line("Before: ");
  serial_write_hex(kernel_heap->start);
  serial_write_hex((ulong)kernel_heap->first_block);
  serial_write_hex((ulong)kernel_heap->first_block->next);
  serial_write_ulong(kernel_heap->first_block->payload_size);
  kmalloc(20000);
  serial_write_line("After: ");
  serial_write_hex(kernel_heap->start);
  serial_write_hex((ulong)kernel_heap->first_block);
  serial_write_hex((ulong)kernel_heap->first_block->next);
  serial_write_ulong(kernel_heap->first_block->payload_size);

  gfx_rect(800, 50, 32, 32, 0x00ff00ff);
  gfx_rect(1000, 50, 32, 32, 0x00ff00ff);

  gfx_rect(800, 150, 32, 50, 0x00ff00ff);
  gfx_rect(1000, 150, 32, 50, 0x00ff00ff);
  gfx_rect(800, 200, 232, 32, 0x00ff00ff);

  for (int x = 0; x < 8; x++) {
    gfx_rect(32 + x * 64, 128, 64, 64, 0x0000ff00);
  }
  for (int x = 0; x < 8; x++) {
    gfx_rect(32 + (x + 1) * 64, 128, 1, 64, 0x00ff0000);
  }
  heap_block *block = kernel_heap->first_block;

  while (block) {
    ulong block_size = block->payload_size + sizeof(heap_block);
    ulong x = 32 + (((ulong)block) - kernel_heap->start) / 128;
    ulong width = block_size / 128;
    gfx_rect(x, 132, width, 56, block->free ? 0x00aeff80 : 0x00ff80ae);
    block = block->next;
  }

  console_write_u32((uint)((kernel_heap->end - kernel_heap->start) / PAGE_SIZE));

  while ((keyboard_last()).code == KEY_NONE) {
    asm volatile("hlt");
  }
}