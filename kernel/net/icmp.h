#pragma once

#include "net_types.h"
#include "types.h"

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

typedef struct {
  ubyte type;
  ubyte code;
  ushort checksum;
  uint rest;
} __attribute__((__packed__)) icmp_header;

void icmp_send_ping(ipv4_addr dst_addr);
void icmp_receive(ipv4_addr src_addr, void *data, ushort data_len);