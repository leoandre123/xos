#pragma once
#include "net/drivers/network_driver.h"
#include "net_types.h"
#include "types.h"

#define MAX_NICS 4

typedef struct nic {
  bool used;
  bool is_up;
  int nic_id;
  ipv4_addr addr;
  ipv4_addr netmask;
  ipv4_addr default_gateway;
  void *conf;
  net_ops *driver;
} nic;

extern nic g_nics[MAX_NICS];

nic *get_nic(int nic_id);
void networking_init();
void configure_nic(int nic_id, nic_config_field field, ulong value);