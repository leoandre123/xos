#pragma once
#include "types.h"

// Initialise the Embedded Controller driver.
// If an ECDT is present in the ACPI tables the ports from it are used;
// otherwise the default PC ports 0x62 (data) / 0x66 (cmd) are used.
void ec_init(void);

// Read/write a single byte from EC address space.
ubyte ec_read(ubyte reg);
void  ec_write(ubyte reg, ubyte val);

// Read a multi-byte little-endian value (2 or 4 bytes) from EC address space.
ushort ec_read16(ubyte reg);
uint   ec_read32(ubyte reg);
