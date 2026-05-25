#pragma once
#include "net_types.h"

typedef struct {
  ipv4_addr destination;
  ipv4_addr netmask;
  ipv4_addr gateway;
  int nic_id;
  int metric;
} route_info;