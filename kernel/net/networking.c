#include "networking.h"
#include "io/e1000.h"
#include "io/logging.h"
#include "io/pci.h"
#include "net/drivers/loopback.h"
#include "net/drivers/network_driver.h"
#include "net/net.h"
#include "net/routing.h"
#include "net_types.h"
#include "types.h"
#include "utils/random.h"

nic g_nics[MAX_NICS] = {0};

static net_ops *get_driver(ushort vendor, ushort device) {
  if (vendor == 0x8086) {
    switch (device) {
    case 0x100E: // Intel 82540EM (QEMU e1000)
    case 0x15D8: // Intel I219-LM (e1000e compatible)
    case 0x10D3:
      return &g_e1000_ops;
    }
  }
  return 0;
}

static int register_nic(net_ops *ops) {
  for (int i = 0; i < MAX_NICS; i++) {
    if (!g_nics[i].used) {
      g_nics[i].nic_id = rand32();
      g_nics[i].used = true;
      g_nics[i].is_up = true;
      g_nics[i].conf = 0;
      g_nics[i].driver = ops;
      g_nics[i].addr = IP(0, 0, 0, 0);
      g_nics[i].netmask = IP(0, 0, 0, 0);
      g_nics[i].default_gateway = IP(0, 0, 0, 0);

      route_upsert((route){
          .destination = IP(255, 255, 255, 255),
          .netmask = IP(255, 255, 255, 255),
          .gateway = IP(0, 0, 0, 0),
          .nic_id = g_nics[i].nic_id,
          .metric = 100,
      });
      return g_nics[i].nic_id;
    }
  }
  return -1;
}

nic *get_nic(int nic_id) {
  for (int i = 0; i < MAX_NICS; i++) {
    if (g_nics[i].used && g_nics[i].nic_id == nic_id) {
      return &g_nics[i];
    }
  }
  return 0;
}

void on_ethernet_controller_found(ubyte bus, ubyte dev, ubyte func) {
  klogf(LOG_DEBUG, "Ethernet controller found at %02x:%02x:%02x", bus, dev, func);

  ushort vendor = pci_get_vendor_id(bus, dev, func);
  ushort device = pci_get_device_id(bus, dev, func);

  net_ops *driver = get_driver(vendor, device);

  if (!driver) {
    klogf(LOG_WARNING, "No driver found for %04x:%04x (%s)", vendor, device, pci_get_vendor_name(vendor));
    return;
  }
  klogf(LOG_INFO, "Driver found for %04x:%04x (%s)", vendor, device, pci_get_vendor_name(vendor));

  if (driver->init)
    driver->init(bus, dev, func);

  register_nic(driver);
}

void networking_init() {
  klogf(LOG_DEBUG, "Initing networking");

  int loopback_nic_id = register_nic(&g_loopback_ops);
  nic *loopback = get_nic(loopback_nic_id);
  loopback->addr = IP(127, 0, 0, 1);
  loopback->netmask = IP(255, 0, 0, 0);
  route_upsert((route){
      .destination = IP(127, 0, 0, 0),
      .netmask = IP(255, 0, 0, 0),
      .gateway = IP(0, 0, 0, 0),
      .nic_id = loopback_nic_id,
      .metric = 10,
  });
  pci_scan_for_class(0x02, 0x00, on_ethernet_controller_found);
}

void configure_nic(int nic_id, nic_config_field field, ulong value) {
  nic *nic = get_nic(nic_id);
  if (!nic)
    return;

  switch (field) {
  case NIC_ADDRESS:
    nic->addr = (ipv4_addr)(uint)value;
    if (nic->netmask.value)
      route_upsert((route){
          .destination = (ipv4_addr)(uint)(nic->addr.value & nic->netmask.value),
          .netmask = nic->netmask,
          .gateway = IP(0, 0, 0, 0),
          .nic_id = nic_id,
          .metric = 0,
      });
    break;
  case NIC_NETMASK:
    nic->netmask = (ipv4_addr)(uint)value;
    if (nic->addr.value)
      route_upsert((route){
          .destination = (ipv4_addr)(uint)(nic->addr.value & nic->netmask.value),
          .netmask = nic->netmask,
          .gateway = IP(0, 0, 0, 0),
          .nic_id = nic_id,
          .metric = 0,
      });
    break;
  case NIC_GATEWAY:
    nic->default_gateway = (ipv4_addr)(uint)value;
    route_upsert((route){
        .destination = IP(0, 0, 0, 0),
        .netmask = IP(0, 0, 0, 0),
        .gateway = nic->default_gateway,
        .nic_id = nic_id,
        .metric = 100,
    });
    break;
  case NIC_METRIC:
    break;
  }
}