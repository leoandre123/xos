#pragma once
#include "types.h"

#define ETHERTYPE_ARP  0x0806
#define ETHERTYPE_IPV4 0x0800

typedef struct {
  ubyte  dst[6];
  ubyte  src[6];
  ushort ethertype; // big-endian
} __attribute__((packed)) ethernet_frame;

// Called by e1000_poll when a packet arrives
void ethernet_receive(ubyte *data, ushort len);

// Send an ethernet frame — fills src MAC automatically
void ethernet_send(ubyte dst[6], ushort ethertype, void *payload, ushort payload_len);
