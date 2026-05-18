#include "elf.h"
#include "filesystem/file.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "memory/pmm.h"
#include "memory/vmm.h"

void *elf_load(file_handle file_handle, address_space *space) {
  if (file_handle->size < sizeof(elf_header)) {
    serial_printf("File to small for an ELF:\n");
    return 0;
  }

  uint magic;
  file_read(file_handle, &magic, 4);

  if (magic != 0x464c457f) {
    serial_printf("Invalid magic for ELF header: %x\n", magic);
    return 0;
  }

  ubyte *data = kmalloc(file_handle->size);
  file_read(file_handle, data, file_handle->size);

  elf_header *hdr = (elf_header *)data;

  if (hdr->magic[0] != 0x7F || hdr->magic[1] != 'E')
    return 0;

  // address_space *space = vmm_create_address_space();

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

  return (void *)entry;
}