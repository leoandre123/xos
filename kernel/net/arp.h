#pragma once
#include "net/networking.h"
#include "net_types.h"
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
  mac_addr sender_mac_address;
  ipv4_addr sender_ip_address;
  mac_addr target_mac_address;
  ipv4_addr target_ip_address;
} __attribute__((__packed__)) arp_packet_ipv4;

typedef struct {
  ipv4_addr ip;
  mac_addr mac;
} arp_entry;

void arp_receive(ubyte *data, ushort len, nic *nic);
void arp_send_ipv4(ipv4_addr addr, nic *nic);

void arp_table_add(ipv4_addr ip, mac_addr mac);
int arp_table_lookup(ipv4_addr ip, mac_addr *mac_out);