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

// Pack/unpack helpers
static inline ipv4_addr ipv4(ubyte a, ubyte b, ubyte c, ubyte d) {
  ipv4_addr ip;
  ip.parts[0] = a;
  ip.parts[1] = b;
  ip.parts[2] = c;
  ip.parts[3] = d;
  return ip;
}
