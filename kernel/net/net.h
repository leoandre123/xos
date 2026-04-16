#pragma once
#include "net_types.h"
#include "types.h"

// Expands IP(10,0,2,2) -> {10, 0, 2, 2}
#define IP(a, b, c, d) ((ipv4_addr)(uint)(d << 24 | c << 16 | b << 8 | a))
#define IP_SET(d, a0, a1, a2, a3) \
  d[0] = (a0);                    \
  d[1] = (a1);                    \
  d[2] = (a2);                    \
  d[3] = (a3);

// typedef ubyte ipv4_addr[4];
// typedef ubyte mac_addr[6];

// Network <-> host byte order (x86 is little-endian, network is big-endian)
static inline ushort htons(ushort x) { return (x >> 8) | (x << 8); }
static inline ushort ntohs(ushort x) { return (x >> 8) | (x << 8); }
static inline uint htonl(uint x) { return ((x & 0xFF) << 24) | (((x >> 8) & 0xFF) << 16) | (((x >> 16) & 0xFF) << 8) | (x >> 24); }
static inline uint ntohl(uint x) { return htonl(x); }