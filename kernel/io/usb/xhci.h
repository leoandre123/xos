#pragma once
#include "types.h"

typedef struct {
  ubyte pci_bus, pci_dev, pci_func;
  ulong mmio_base;

  volatile uint *cap;
  volatile uint *op;

  ubyte max_ports;
  ubyte max_slots;
} xhci_controller;

void xhci_init_controller(ubyte bus, ubyte dev, ubyte func);