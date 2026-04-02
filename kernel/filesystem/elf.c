#include "elf.h"
#include "filesystem/fat32.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "memory/pmm.h"
#include "memory/vmm.h"
#include "scheduler/scheduler.h"
#include "scheduler/task.h"

task *elf_load(fat32_file *file_handle, const char *name, const char *wd) {
  ubyte *data = kmalloc(file_handle->size);
  fat32_read(file_handle, data, file_handle->size);

  elf_header *hdr = (elf_header *)data;

  if (hdr->magic[0] != 0x7F || hdr->magic[1] != 'E')
    return 0;

  address_space *space = vmm_create_address_space();

  for (int i = 0; i < hdr->program_entry_count; i++) {
    elf_program_header *prg_hdr = (elf_program_header *)(data + hdr->program_offset + i * hdr->program_entry_size);

    if (prg_hdr->type != 1)
      continue;

    uint pages = (prg_hdr->size_mem + PAGE_SIZE - 1) / PAGE_SIZE;
    ulong phys_addr = pmm_alloc_pages(pages);

    ulong page_flags = PAGE_PRESENT | PAGE_USER;
    if (prg_hdr->flags & 2)
      page_flags |= PAGE_WRITABLE;

    vmm_map_pages(space, prg_hdr->vaddr, phys_addr, pages, page_flags);

    void *dest = PHYS_TO_HHDM(phys_addr);
    memcpy8(dest, data + prg_hdr->offset, prg_hdr->size_file);
    memset8(dest + prg_hdr->size_file, 0, prg_hdr->size_mem - prg_hdr->size_file);
  }

  ulong entry = hdr->program_entry;
  kfree(data);

  return task_create_user_from_space(space, (void *)entry, name, wd);
}