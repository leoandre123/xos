#pragma once
#include "types.h"

void xhci_init(void);
void xhci_poll(void);
bool usb_msc_ok(void);
bool usb_msc_read(uint lba, ubyte count, void *buf);
