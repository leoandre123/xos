#pragma once
#include "net/net.h"
#include "types.h"

#define ARP_REQUEST 1
#define ARP_REPLY   2

#define ARP_TABLE_SIZE 16

typedef struct {
  ushort hardware_type;
  ushort protocol_type;
  ubyte hardware_length;
  ubyte protocol_length;
  ushort op_code;
  ubyte sender_mac_address[6];
  ubyte sender_ip_address[4];
  ubyte target_mac_address[6];
  ubyte target_ip_address[4];
} __attribute__((__packed__)) arp_packet_ipv4;

typedef struct {
  ipv4_addr ip;
  mac_addr mac;
} arp_entry;

void arp_receive(ubyte *data, ushort len);
void arp_send_ipv4(ubyte addr[4]);

void arp_table_add(ipv4_addr ip, mac_addr mac);
int arp_table_lookup(ipv4_addr ip, mac_addr mac_out);