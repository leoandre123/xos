#include "pci.h"
#include "io/io.h"
#include "io/serial.h"
#include "types.h"

uint pci_read32(ubyte bus, ubyte dev, ubyte func, ubyte offset) {
  uint addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC);
  outl(0xCF8, addr);
  return inl(0xCFC);
}

void pci_write32(ubyte bus, ubyte dev, ubyte func, ubyte offset, uint value) {
  uint addr = (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (offset & 0xFC);
  outl(0xCF8, addr);
  outl(0xCFC, value);
}
ulong pci_get_bar0(ubyte bus, ubyte dev, ubyte func) {
  uint bar = pci_read32(bus, dev, func, 0x10);
  // bit 0 = 0 means MMIO, bit 0 = 1 means I/O port
  // mask off the low 4 flag bits to get the base address
  return (ulong)(bar & ~0xF);
}

void pci_enable_bus_master(ubyte bus, ubyte dev, ubyte func) {
  uint cmd = pci_read32(bus, dev, func, 0x04);
  cmd |= (1 << 1); // memory space enable
  cmd |= (1 << 2); // bus master enable
  pci_write32(bus, dev, func, 0x04, cmd);
}

void pci_scan(void) {
  for (int bus = 0; bus < 256; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      uint id = pci_read32(bus, dev, 0, 0);
      if ((id & 0xFFFF) == 0xFFFF)
        continue; // no device

      ushort vendor = id & 0xFFFF;
      ushort device = id >> 16;

      uint class = pci_read32(bus, dev, 0, 0x08);
      ubyte class_code = class >> 24;
      ubyte subclass = (class >> 16) & 0xFF;

      serial_write("PCI: ");
      serial_write_hex8(bus);
      serial_write_char(':');
      serial_write_hex8(dev);

      serial_write(" vendor = ");
      serial_write_hex16(vendor);
      serial_write(" device = ");
      serial_write_hex16(device);
      serial_write(" class = ");

      serial_write_hex8(class_code);
      serial_write_char(':');
      serial_write_hex8(subclass);
      serial_write_char('\n');
    }
  }
}