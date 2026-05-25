#pragma once
#include "net/drivers/network_driver.h"
#include "net/networking.h"
#include "net_types.h"
#include "types.h"

extern net_ops g_e1000_ops;

void e1000_init(ubyte bus, ubyte dev, ubyte func);
void e1000_send(void *data, ushort len);
void e1000_poll(nic *nic);
void e1000_get_mac(mac_addr *mac_out);
