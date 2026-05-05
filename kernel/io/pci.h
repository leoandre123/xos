#pragma once

#include "types.h"

uint pci_read32(ubyte bus, ubyte dev, ubyte func, ubyte offset);
void pci_write32(ubyte bus, ubyte dev, ubyte func, ubyte offset, uint value);
void pci_scan(void);
ulong pci_get_bar(ubyte bus, ubyte dev, ubyte func);
void pci_enable_bus_master(ubyte bus, ubyte dev, ubyte func);
void pci_scan_for_class(ubyte class, ubyte subclass, void (*callback)(ubyte bus, ubyte dev, ubyte func));

const char *pci_get_vendor_name(ushort vendor_id);

static inline uint pci_get_vendor_id(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0) & 0xFFFF;
}

static inline uint pci_get_device_id(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0) >> 16;
}

static inline ubyte pci_get_progif(ubyte bus, ubyte dev, ubyte func) {
  return (pci_read32(bus, dev, func, 0x08) >> 8) & 0xFF;
}

static inline ulong pci_get_bar0(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0x10);
}
static inline ulong pci_get_bar1(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0x14);
}
static inline ulong pci_get_bar2(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0x18);
}
static inline ulong pci_get_bar3(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0x1C);
}
static inline ulong pci_get_bar4(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0x20);
}
static inline ulong pci_get_bar5(ubyte bus, ubyte dev, ubyte func) {
  return pci_read32(bus, dev, func, 0x24);
}