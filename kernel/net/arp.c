#include "arp.h"
#include "io/e1000.h"
#include "io/serial.h"
#include "memory/memutils.h"
#include "net/ethernet.h"
#include "net/ip.h"
#include "net/net.h"
#include "types.h"

static arp_entry arp_table[ARP_TABLE_SIZE] = {0};

void arp_receive(ubyte *data, ushort len) {

  if (len < sizeof(arp_packet_ipv4)) {
    serial_write("Packet to small for ARP");
    return;
  }

  arp_packet_ipv4 *packet = (arp_packet_ipv4 *)data;
  ubyte *ipv4 = packet->sender_ip_address;
  ubyte *mac = packet->sender_mac_address;

  arp_table_add(ipv4, mac);

  ip_send_pending(ipv4);

  serial_printf("ARP: %1d.%1d.%1d.%1d is at %02x:%02x:%02x:%02x:%02x:%02x\n",
                ipv4[0], ipv4[1], ipv4[2], ipv4[3], mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void arp_send_ipv4(ubyte addr[4]) {

  arp_packet_ipv4 arp_packet;
  arp_packet.hardware_type = htons(1);
  arp_packet.protocol_type = htons(ETHERTYPE_IPV4);
  arp_packet.hardware_length = 6;
  arp_packet.protocol_length = 4;
  arp_packet.op_code = htons(ARP_REQUEST);
  e1000_get_mac(arp_packet.sender_mac_address);
  arp_packet.sender_ip_address[0] = 10;
  arp_packet.sender_ip_address[1] = 0;
  arp_packet.sender_ip_address[2] = 2;
  arp_packet.sender_ip_address[3] = 15;
  memset8(arp_packet.target_mac_address, 0x00, 6);
  memcpy8(arp_packet.target_ip_address, addr, 4);

  ubyte broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  ethernet_send(broadcast, ETHERTYPE_ARP, &arp_packet, sizeof(arp_packet_ipv4));
}

void arp_table_add(ipv4_addr ip, mac_addr mac) {
  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table[i].ip[0] == 0) {
      memcpy8(arp_table[i].ip, ip, 4);
      memcpy8(arp_table[i].mac, mac, 4);
      return;
    }
  }
}
int arp_table_lookup(ipv4_addr ip, mac_addr mac_out) {

  if (ip[0] == 255 &&
      ip[1] == 255 &&
      ip[2] == 255 &&
      ip[3] == 255) {
    memset8(mac_out, 0xff, 6);
    return 1;
  }

  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (ipv4_cmp(arp_table[i].ip, ip)) {
      memcpy8(mac_out, arp_table[i].mac, 4);
      return 1;
    }
  }
  return 0;
}