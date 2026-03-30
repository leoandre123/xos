#pragma once

#include "filesystem/fat32.h"
#include "scheduler/task.h"
#include "types.h"

#define ELF_x86     0x03
#define ELF_ARM     0x28
#define ELF_x86_64  0x3E
#define ELF_AArch64 0xB7

typedef struct {
  ubyte magic[4];
  ubyte class;
  ubyte endianness;
  ubyte header_version;
  ubyte abi;
  ubyte padding[8];
  ushort type;
  ushort machine;
  uint version;
  ulong program_entry;
  ulong program_offset;
  ulong section_offset;
  uint flags;
  ushort header_size;
  ushort program_entry_size;
  ushort program_entry_count;
  ushort section_entry_size;
  ushort section_entry_count;
  ushort section_index;

} __attribute__((packed)) elf_header;

typedef struct {
  uint type;
  uint flags;
  ulong offset;
  ulong vaddr;
  ulong paddr;
  ulong size_file;
  ulong size_mem;
  ulong alignment;
} __attribute__((packed)) elf_program_header;

task *elf_load(fat32_file *file_handle);