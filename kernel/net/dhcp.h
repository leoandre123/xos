#pragma once

#include "net/net.h"
#include "types.h"
typedef struct {
  ubyte op;
  ubyte htype;
  ubyte hlen;
  ubyte hops;
  uint xid;
  ushort secs;
  ushort flags;
  ipv4_addr client_addr;
  ipv4_addr your_addr;
  ipv4_addr server_addr;
  ipv4_addr gateway_addr;
  ubyte client_hw_addr[16];
  ubyte overflow[192];
  uint magic_cookie;
  ubyte options[64];
} __attribute__((__packed__)) dhcp_packet;

void dhcp_send_discovery();

void dhcp_receive(void *data, ushort data_len);