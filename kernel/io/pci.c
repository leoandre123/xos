#include "pci.h"
#include "io/io.h"
#include "io/logging.h"
#include "io/serial.h"
#include "types.h"

#define CASE_CLASS_NAME(c, sc, n) \
  case ((c << 8) | sc):           \
    return n
#define CASE_VENDOR_NAME(id, n) \
  case id:                      \
    return n

const char *pci_get_vendor_name(ushort vendor_id) {
  switch (vendor_id) {
    CASE_VENDOR_NAME(0x10ec, "Realtek");
    CASE_VENDOR_NAME(0x8086, "Intel");
  default:
    return "Unknown";
  }
}

static const char *pci_get_name(ubyte class, ubyte subclass) {
  ushort combination = (((ushort)class << 8) | subclass);

  switch (combination) {
    CASE_CLASS_NAME(0, 0, "Non-VGA-Compatible Unclassified Device");
    CASE_CLASS_NAME(0, 1, "VGA-Compatible Unclassified Device");

    CASE_CLASS_NAME(1, 0, "SCSI Bus Controller");
    CASE_CLASS_NAME(1, 1, "IDE Controller");
    CASE_CLASS_NAME(1, 2, "Floppy Disk Controller");
    CASE_CLASS_NAME(1, 3, "IPI Bus Controller");
    CASE_CLASS_NAME(1, 4, "RAID Controller");
    CASE_CLASS_NAME(1, 5, "ATA Controller");
    CASE_CLASS_NAME(1, 6, "Serial ATA Controller");
    CASE_CLASS_NAME(1, 7, "Serial Attached SCSI Controller");
    CASE_CLASS_NAME(1, 8, "Non-Volatile Memory Controller");

    CASE_CLASS_NAME(2, 0, "Ethernet Controller");
    CASE_CLASS_NAME(2, 1, "Token Ring Controller");
    CASE_CLASS_NAME(2, 2, "FDDI Controller");
    CASE_CLASS_NAME(2, 3, "ATM Controller");
    CASE_CLASS_NAME(2, 4, "ISDN Controller");
    CASE_CLASS_NAME(2, 5, "WorldFip Controller");
    CASE_CLASS_NAME(2, 6, "PICMG 2.14 Multi Computing Controller");
    CASE_CLASS_NAME(2, 7, "Infiniband Controller");
    CASE_CLASS_NAME(2, 8, "Fabric Controller");

    CASE_CLASS_NAME(3, 0, "VGA Compatible Controller");
    CASE_CLASS_NAME(3, 1, "XGA Controller");
    CASE_CLASS_NAME(3, 2, "3D Controller (Not VGA-Compatible)");

    CASE_CLASS_NAME(4, 0, "Multimedia Video Controller");
    CASE_CLASS_NAME(4, 1, "Multimedia Audio Controller");
    CASE_CLASS_NAME(4, 2, "Computer Telephony Device");
    CASE_CLASS_NAME(4, 3, "Audio Device");

    CASE_CLASS_NAME(5, 0, "RAM Controller");
    CASE_CLASS_NAME(5, 1, "Flash Controller");

    CASE_CLASS_NAME(0xC, 0, "FireWire (IEEE 1394) Controller");
    CASE_CLASS_NAME(0xC, 1, "ACCESS Bus Controller");
    CASE_CLASS_NAME(0xC, 2, "SSA");
    CASE_CLASS_NAME(0xC, 3, "USB Controller");
    CASE_CLASS_NAME(0xC, 4, "Fibre Channel");
    CASE_CLASS_NAME(0xC, 5, "SMBus Controller");
    CASE_CLASS_NAME(0xC, 6, "InfiniBand Controller");
    CASE_CLASS_NAME(0xC, 7, "IPMI Interface");
    CASE_CLASS_NAME(0xC, 8, "SERCOS Interface (IEC 61491)");
    CASE_CLASS_NAME(0xC, 9, "CANbus Controller");
  default:
    return "Unknown";
  }
}

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

ulong pci_get_bar(ubyte bus, ubyte dev, ubyte func) {
  uint bar0 = pci_read32(bus, dev, func, 0x10);

  if ((bar0 & 0x06) == 0x04) { // 64-bit BAR
    uint bar1 = pci_read32(bus, dev, func, 0x14);
    return ((ulong)bar1 << 32) | (bar0 & ~0xFUL);
  } else {
    return bar0 & ~0xFUL;
  }
}

void pci_enable_bus_master(ubyte bus, ubyte dev, ubyte func) {
  uint cmd = pci_read32(bus, dev, func, 0x04);
  cmd |= (1 << 1); // memory space enable
  cmd |= (1 << 2); // bus master enable
  pci_write32(bus, dev, func, 0x04, cmd);
}

static void pci_check_function(ubyte bus, ubyte dev, ubyte func) {
  uint id = pci_read32(bus, dev, func, 0);
  if ((id & 0xFFFF) == 0xFFFF)
    return;

  ushort vendor = id & 0xFFFF;
  ushort device = id >> 16;

  uint class = pci_read32(bus, dev, func, 0x08);
  ubyte class_code = class >> 24;
  ubyte subclass = (class >> 16) & 0xFF;

  klogf(LOG_DEBUG, "PCI: %02x:%02x:%02x ven=%04x dev=%04x cls: %02x:%02x (%s)",
        bus, dev, func, vendor, device, class_code, subclass, pci_get_name(class_code, subclass));
}

static void pci_check_device(ubyte bus, ubyte dev) {
  uint id = pci_read32(bus, dev, 0, 0);
  if ((id & 0xFFFF) == 0xFFFF)
    return;

  uint header = pci_read32(bus, dev, 0, 0x0C);
  ubyte header_type = (header >> 16) & 0xFF;

  pci_check_function(bus, dev, 0);

  if (header_type & 0x80) {
    for (int func = 1; func < 8; func++) {
      pci_check_function(bus, dev, func);
    }
  }
}

void pci_scan(void) {
  for (int bus = 0; bus < 256; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      pci_check_device(bus, dev);
    }
  }
}

void pci_scan_for_class(ubyte class, ubyte subclass, void (*callback)(ubyte bus, ubyte dev, ubyte func)) {
  for (int bus = 0; bus < 256; bus++) {
    for (int dev = 0; dev < 32; dev++) {
      uint id = pci_read32(bus, dev, 0, 0);
      if ((id & 0xFFFF) == 0xFFFF)
        continue;

      uint header = pci_read32(bus, dev, 0, 0x0C);
      ubyte header_type = (header >> 16) & 0xFF;
      ubyte func_count = header_type & 0x80 ? 8 : 1;

      for (int func = 0; func < func_count; func++) {
        id = pci_read32(bus, dev, func, 0);
        if ((id & 0xFFFF) == 0xFFFF)
          continue;
        ;

        uint cls = pci_read32(bus, dev, func, 0x08);
        ubyte class_code = cls >> 24;
        ubyte subclass_code = (cls >> 16) & 0xFF;

        if (class_code == class && subclass_code == subclass) {
          callback(bus, dev, func);
        }
      }
    }
  }
}