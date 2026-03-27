#pragma once
#include "types.h"

#define HEAP_ALIGN_SIZE   0x10
#define HEAP_INITAL_PAGES 0x04
#define HEAP_EXPAND_PAGES 0x01

typedef struct heap_block {
  ulong payload_size;
  struct heap_block *next;
  struct heap_block *prev;
  ubyte free;
} heap_block;
typedef struct {
  ulong start;
  ulong end;
  heap_block *first_block;
} heap;

void heap_init();
void *kmalloc(ulong size);
void kfree(void *addr);

heap *get_kernel_heap();