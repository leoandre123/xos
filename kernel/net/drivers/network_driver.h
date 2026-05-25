#pragma once
#include "net_types.h"
#include "types.h"

struct nic;

typedef struct net_ops {
  void (*init)(ubyte bus, ubyte dev, ubyte func);
  void (*poll)(struct nic *nic);
  void (*send)(struct nic *nic, void *data, ushort len, ipv4_addr gateway);
  void (*get_mac)(mac_addr *mac_out);
  void (*transmit)(void *data, ushort len);
} net_ops;
