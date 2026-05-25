#pragma once
#include "types.h"

typedef union {
  uint value;
  ubyte parts[4];
} ipv4_addr;

typedef union {
  ulong value;
  ubyte parts[6];
} ipv6_addr;

typedef union {
  ubyte parts[6];
} mac_addr;

typedef enum : ubyte {
  SOCKET_RAW = 0,
  SOCKET_ICMP = 1,
  SOCKET_TCP = 6,
  SOCKET_UDP = 17,
} socket_protocol;

typedef enum : ubyte {
  NIC_ADDRESS,
  NIC_NETMASK,
  NIC_GATEWAY,
  NIC_METRIC,
} nic_config_field;

typedef struct {
  socket_protocol protocol;
  union {
    struct {
      ipv4_addr addr;
      ushort port;
    } tcp_addr;
    struct {
      ipv4_addr addr;
      ushort port;
    } udp_addr;
  };
} socket_addr;

// Pack/unpack helpers
static inline ipv4_addr ipv4(ubyte a, ubyte b, ubyte c, ubyte d) {
  ipv4_addr ip;
  ip.parts[0] = a;
  ip.parts[1] = b;
  ip.parts[2] = c;
  ip.parts[3] = d;
  return ip;
}

#define IPV4_SPILL(ip) ip.parts[0], \
                       ip.parts[1], \
                       ip.parts[2], \
                       ip.parts[3]