#pragma once
#include "net/net.h"
#include "types.h"

#define IP_MAX_PACKET_SIZE     1500
#define IP_MAX_PENDING_PACKETS 16

#define PROTOCOL_ICMP 1
#define PROTOCOL_IGMP 2
#define PROTOCOL_TCP  6
#define PROTOCOL_UDP  17

typedef struct {
  ubyte version_ihl;
  ubyte dscp_ecn;
  ushort total_length;
  ushort identification;
  ushort flags_fragment_offset;
  ubyte ttl;
  ubyte protocol;
  ushort header_checksum;
  ipv4_addr src_addr;
  ipv4_addr dst_addr;
} __attribute__((__packed__)) ipv4_header;

void ip_receive(void *data, ushort data_len);
void ip_send(ipv4_addr dst_addr, ubyte protocol, void *payload, ushort payload_len);
void ip_send_pending(ipv4_addr dst_addr);