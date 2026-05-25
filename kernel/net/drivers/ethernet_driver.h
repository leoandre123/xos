#pragma once
#include "net/networking.h"
#include "net_types.h"
#include "types.h"

void ethernet_driver_send(nic *nic, void *data, ushort len, ipv4_addr gateway);
void ethernet_driver_flush_pending(ipv4_addr resolved);