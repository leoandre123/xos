#pragma once

#include "types.h"
uint pci_read32(ubyte bus, ubyte dev, ubyte func, ubyte offset);
void pci_write32(ubyte bus, ubyte dev, ubyte func, ubyte offset, uint value);
void pci_scan(void);
ulong pci_get_bar0(ubyte bus, ubyte dev, ubyte func);
void pci_enable_bus_master(ubyte bus, ubyte dev, ubyte func);