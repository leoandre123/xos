#pragma once
#include "net/drivers/network_driver.h"
#include "net_types.h"
#include "types.h"

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

typedef struct {
  mac_addr dst;
  mac_addr src;
  ushort ethertype; // big-endian
} __attribute__((packed)) ethernet_frame;

// Called by e1000_poll when a packet arrives
void ethernet_receive(ubyte *data, ushort len, struct nic *nic);

// Send an ethernet frame — fills src MAC automatically
void ethernet_send(mac_addr dst, ushort ethertype, void *payload, ushort payload_len, net_ops *driver);
