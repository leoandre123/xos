#pragma once
#include "types.h"

// Common ACPI SDT header (36 bytes)
typedef struct {
  char sig[4];
  uint length;
  ubyte revision;
  ubyte checksum;
  char oem_id[6];
  char oem_table_id[8];
  uint oem_revision;
  uint creator_id;
  uint creator_revision;
} __attribute__((packed)) acpi_header;

// RSDP (Root System Description Pointer)
typedef struct {
  char sig[8]; // "RSD PTR "
  ubyte checksum;
  char oem_id[6];
  ubyte revision;
  uint rsdt_phys;
  // ACPI 2.0+ extension (present when revision >= 2)
  uint length;
  ulong xsdt_phys;
  ubyte ext_checksum;
  ubyte reserved[3];
} __attribute__((packed)) acpi_rsdp;

void acpi_init(ulong rsdp_phys);

// Return the n-th table matching the 4-char signature (0-based), or NULL.
// Pointer is in the HHDM — safe to dereference.
acpi_header *acpi_find_table(const char *sig4);
acpi_header *acpi_find_table_nth(const char *sig4, uint n);

// Physical address of the DSDT (0 if not found)
ulong acpi_get_dsdt_phys(void);
