#include "usb.h"
#include "io/logging.h"
#include "io/pci.h"
#include "io/usb/xhci.h"

typedef struct {
  ubyte address;
  ubyte class_code;
  ubyte subclass;
  ubyte protocol;
  ushort vendor_id;
  ushort product_id;
  bool is_hub;
} usb_device;

void on_usb_controller_found(ubyte bus, ubyte dev, ubyte func) {
  ubyte prog_if = pci_get_progif(bus, dev, func);
  klogf(LOG_DEBUG, "USB controller found. Type: %02x", prog_if);

  if (prog_if != 0x30)
    return;

  xhci_init_controller(bus, dev, func);
}

void usb_init() {
  pci_scan_for_class(0xC, 0x3, on_usb_controller_found);
}