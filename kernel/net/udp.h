#pragma once

#include "net/net.h"
#include "types.h"
typedef struct {
  ushort src_port;
  ushort dst_port;
  ushort length;
  ushort checksum;
} udp_header;

void udp_send(ipv4_addr dst_addr, ushort src_port, ushort dst_port, void *payload, ushort payload_len);
void udp_receive(void *data, ushort data_len);