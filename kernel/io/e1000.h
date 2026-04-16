#pragma once
#include "net_types.h"
#include "types.h"

void e1000_init(ulong mmio_phys);
void e1000_send(void *data, ushort len);
void e1000_poll(void);
void e1000_get_mac(mac_addr *mac_out);
