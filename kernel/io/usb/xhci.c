
#include "xhci.h"
#include "io/logging.h"
#include "io/pci.h"
#include "memory/vmm.h"
#include "types.h"

#define XHCI_CAP_LENGTH 0x00
#define XHCI_HCSPARAMS1 0x04

#define XHCI_USBCMD 0x00
#define XHCI_USBSTS 0x04
#define XHCI_CONFIG 0x38

#define XHCI_CMD_RUN   (1 << 0)
#define XHCI_CMD_RESET (1 << 1)

#define XHCI_STS_HALT (1 << 0)
#define XHCI_STS_CNR  (1 << 11)

#define XHCI_PORTSC_BASE     0x400
#define XHCI_PORT_REG_STRIDE 0x10

#define PORTSC_CCS         (1 << 0) // Current Connect Status
#define PORTSC_PED         (1 << 1) // Port Enabled/Disabled
#define PORTSC_PR          (1 << 4) // Port Reset
#define PORTSC_PLS_MASK    (0xF << 5)
#define PORTSC_PP          (1 << 9) // Port Power
#define PORTSC_SPEED_SHIFT 10
#define PORTSC_SPEED_MASK  (0xF << 10)
#define PORTSC_CSC         (1 << 17) // Connect Status Change
#define PORTSC_PRC         (1 << 21) // Port Reset Change

xhci_controller g_controller = {0};

static inline void xhci_wait(volatile uint *reg, uint mask, bool set) {
  for (int i = 0; i < 1000000; i++) {
    if (((*reg & mask) != 0) == set)
      return;
  }
}

static xhci_controller *alloc_controller() {
  return &g_controller;
}

static volatile uint *xhci_portsc(xhci_controller *ctrl, uint port_index) {
  return (volatile uint *)((ulong)ctrl->op + XHCI_PORTSC_BASE +
                           port_index * XHCI_PORT_REG_STRIDE);
}

void xhci_reset_port(xhci_controller *ctrl, uint port_index) {
  volatile uint *port = xhci_portsc(ctrl, port_index);

  uint v = *port;

  if (!(v & PORTSC_CCS)) {
    klogf(LOG_DEBUG, "xHCI port %u: no device connected", port_index + 1);
    return;
  }

  klogf(LOG_DEBUG, "xHCI port %u before reset: %08x", port_index + 1, v);

  // Start port reset.
  *port = v | PORTSC_PR;

  // Wait for reset to complete.
  for (int i = 0; i < 1000000; i++) {
    v = *port;
    if (v & PORTSC_PRC)
      break;
  }

  // Clear reset-change bit by writing 1 to it.
  *port = v | PORTSC_PRC;

  v = *port;

  klogf(LOG_DEBUG,
        "xHCI port %u after reset: %08x connected=%d enabled=%d speed=%u",
        port_index + 1,
        v,
        !!(v & PORTSC_CCS),
        !!(v & PORTSC_PED),
        (v & PORTSC_SPEED_MASK) >> PORTSC_SPEED_SHIFT);
}

void xhci_list_ports(xhci_controller *ctrl) {
  for (uint i = 0; i < ctrl->max_ports; i++) {
    volatile uint *portsc =
        (volatile uint *)((ulong)ctrl->op + XHCI_PORTSC_BASE +
                          i * XHCI_PORT_REG_STRIDE);

    uint v = *portsc;

    bool connected = v & PORTSC_CCS;
    bool enabled = v & PORTSC_PED;
    uint speed = (v & PORTSC_SPEED_MASK) >> PORTSC_SPEED_SHIFT;

    klogf(LOG_DEBUG,
          "xHCI port %d: PORTSC=%08x connected=%d enabled=%d speed=%d",
          i + 1, v, connected, enabled, speed);
  }
}

void xhci_init_controller(ubyte bus, ubyte dev, ubyte func) {
  pci_enable_bus_master(bus, dev, func);

  ulong mmio_phys = pci_get_bar(bus, dev, func);
  ulong mmio_virt = (ulong)PHYS_TO_HHDM(mmio_phys);
  vmm_map_bytes(&g_kernel_address_space,
                mmio_virt, mmio_phys,
                1024 * 1024, PAGE_PRESENT | PAGE_WRITABLE | PAGE_CACHE_DISABLE);

  klogf(LOG_TRACE, "XHCI MMIO (physical): %d", mmio_phys);
  klogf(LOG_TRACE, "XHCI MMIO (virtual): %u", mmio_virt);
  xhci_controller *ctrl = alloc_controller();
  ctrl->mmio_base = mmio_virt;
  ctrl->pci_bus = bus;
  ctrl->pci_dev = dev;
  ctrl->pci_func = func;
  ctrl->cap = (volatile uint *)ctrl->mmio_base;
  klogf(LOG_TRACE, "2");
  ubyte cap_length = *((volatile ubyte *)(ctrl->mmio_base + XHCI_CAP_LENGTH));
  // 2. Operational registers start after cap length
  klogf(LOG_TRACE, "3");
  ctrl->op = (volatile uint *)(ctrl->mmio_base + cap_length);
  klogf(LOG_TRACE, "4");
  // 3. Read structural params
  uint hcsparams1 = ctrl->cap[XHCI_HCSPARAMS1 / 4];

  ctrl->max_slots = (hcsparams1 >> 0) & 0xFF;
  ctrl->max_ports = (hcsparams1 >> 24) & 0xFF;
  klogf(LOG_TRACE, "5");
  // limit slots for safety (you don’t need all early on)
  if (ctrl->max_slots > 32)
    ctrl->max_slots = 32;

  // 4. Halt controller
  ctrl->op[XHCI_USBCMD / 4] &= ~XHCI_CMD_RUN;
  klogf(LOG_TRACE, "6");
  xhci_wait(&ctrl->op[XHCI_USBSTS / 4], XHCI_STS_HALT, true);

  // 5. Reset controller
  ctrl->op[XHCI_USBCMD / 4] |= XHCI_CMD_RESET;
  klogf(LOG_TRACE, "7");
  xhci_wait(&ctrl->op[XHCI_USBCMD / 4], XHCI_CMD_RESET, false);
  xhci_wait(&ctrl->op[XHCI_USBSTS / 4], XHCI_STS_CNR, false);

  // 6. Set max device slots
  ctrl->op[XHCI_CONFIG / 4] = ctrl->max_slots;
  klogf(LOG_TRACE, "8");
  // 7. Run controller
  ctrl->op[XHCI_USBCMD / 4] |= XHCI_CMD_RUN;

  xhci_wait(&ctrl->op[XHCI_USBSTS / 4], XHCI_STS_HALT, false);
  klogf(LOG_TRACE, "9");
  klogf(LOG_DEBUG, "XHCI Controller initialized. Max ports: %d", ctrl->max_ports);

  xhci_list_ports(ctrl);
  for (int i = 0; i < ctrl->max_ports; i++) {
    xhci_reset_port(ctrl, i);
  }
  xhci_list_ports(ctrl);
  // xhci_enumerate_root_ports(&ctrl);
}