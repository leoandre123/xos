#include "networking.h"
#include "io/e1000.h"
#include "io/logging.h"
#include "io/pci.h"
#include "net/drivers/ethernet_driver.h"
#include "types.h"

ed_ops *g_main_driver;

ed_ops e1000_ops = {
    .init = e1000_init,
    .get_mac = e1000_get_mac,
    .poll = e1000_poll,
    .send = e1000_send,
};

static ed_ops *get_driver(ushort vendor, ushort device) {
  if (vendor == 0x8086) {
    switch (device) {
    case 0x100E: // Intel 82540EM (QEMU e1000)
    case 0x15D8: // Intel I219-LM (e1000e compatible)
    case 0x10D3:
      return &e1000_ops;
    }
  }
  return 0;
}

void on_ethernet_controller_found(ubyte bus, ubyte dev, ubyte func) {
  klogf(LOG_DEBUG, "Ethernet controller found at %02x:%02x:%02x", bus, dev, func);

  ushort vendor = pci_get_vendor_id(bus, dev, func);
  ushort device = pci_get_device_id(bus, dev, func);

  ed_ops *driver = get_driver(vendor, device);

  if (!driver) {
    klogf(LOG_WARNING, "No driver found for %04x:%04x (%s)", vendor, device, pci_get_vendor_name(vendor));
    return;
  }
  klogf(LOG_INFO, "Driver found for %04x:%04x (%s)", vendor, device, pci_get_vendor_name(vendor));

  pci_enable_bus_master(bus, dev, func);
  ulong mmio = pci_get_bar(bus, dev, func);

  driver->init(mmio);

  g_main_driver = driver;
}

void networking_init() {
  klogf(LOG_DEBUG, "Initing networking");
  pci_scan_for_class(0x02, 0x00, on_ethernet_controller_found);
}
