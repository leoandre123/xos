#pragma once

#include "net_types.h"
#include "types.h"
typedef struct {
  bool is_up;
  int nic_id;
  ipv4_addr addr;
  ipv4_addr netmask;
  ipv4_addr default_gateway;
} nic_info;