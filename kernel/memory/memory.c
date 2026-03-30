#include "memory.h"
#include "graphics/console.h"

static const char *efi_mem_type_name(uint type) {
  switch (type) {
  case 0:
    return "Reserved";
  case 1:
    return "LoaderCode";
  case 2:
    return "LoaderData";
  case 3:
    return "BootServicesCode";
  case 4:
    return "BootServicesData";
  case 5:
    return "RuntimeServicesCode";
  case 6:
    return "RuntimeServicesData";
  case 7:
    return "Conventional";
  case 8:
    return "Unusable";
  case 9:
    return "ACPIReclaim";
  case 10:
    return "ACPINVS";
  case 11:
    return "MMIO";
  case 12:
    return "MMIOPortSpace";
  case 13:
    return "PalCode";
  case 14:
    return "PersistentMemory";
  default:
    return "Unknown";
  }
}

ulong memory_get_total_usable_bytes(BootInfo *boot_info) {
  ulong total = 0;
  ubyte *cur = (ubyte *)(ulong)boot_info->memory_map;
  ubyte *end = cur + boot_info->memory_map_size;

  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;
    if (desc->Type == 7) { // EfiConventionalMemory
      total += desc->NumberOfPages * 4096ULL;
    }
    cur += boot_info->memory_map_desc_size;
  }

  return total;
}

void memory_map_print(BootInfo *boot_info) {
  console_write("UEFI memory map:\n");

  ubyte *cur = (ubyte *)(ulong)boot_info->memory_map;
  ubyte *end = cur + boot_info->memory_map_size;

  while (cur < end) {
    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)cur;

    console_write(" type=");
    console_write(efi_mem_type_name(desc->Type));
    console_write(" phys=0x");
    console_write_hex64(desc->PhysicalStart);
    console_write(" pages=");
    console_write_u32(desc->NumberOfPages);
    console_write(" bytes=");
    console_write_hex64(desc->NumberOfPages * 4096ULL);
    console_write("\n");

    cur += boot_info->memory_map_desc_size;
  }

  console_write("Total usable RAM: 0x");
  console_write_hex64(memory_get_total_usable_bytes(boot_info));
  console_write("\n");
}