#include "arp.h"
#include "io/e1000.h"
#include "io/logging.h"
#include "memory/memutils.h"
#include "net/dhcp.h"
#include "net/ethernet.h"
#include "net/ip.h"
#include "net/net.h"
#include "net_types.h"
#include "types.h"

static arp_entry arp_table[ARP_TABLE_SIZE] = {0};

static void arp_reply_ipv4(ipv4_addr from_addr, mac_addr from_mac) {
  arp_packet_ipv4 arp_packet = {
      .hardware_type = htons(1),
      .protocol_type = htons(ETHERTYPE_IPV4),
      .hardware_length = 6,
      .protocol_length = 4,
      .op_code = htons(ARP_REPLY),
      //.sender_mac_address = 0,
      .sender_ip_address = g_ip,
      .target_mac_address = from_mac,
      .target_ip_address = from_addr,
  };
  e1000_get_mac(&arp_packet.sender_mac_address);

  ethernet_send(from_mac, ETHERTYPE_ARP, &arp_packet, sizeof(arp_packet_ipv4));
}

void arp_receive(ubyte *data, ushort len) {
  if (len < sizeof(arp_packet_ipv4))
    return;

  arp_packet_ipv4 *packet = (arp_packet_ipv4 *)data;
  ipv4_addr sender_ip = packet->sender_ip_address;
  mac_addr  sender_mac = packet->sender_mac_address;
  ushort op = ntohs(packet->op_code);

  if (op == ARP_REPLY) {
    arp_table_add(sender_ip, sender_mac);
    ip_send_pending(sender_ip);
    klogf(LOG_DEBUG, "ARP reply: %d.%d.%d.%d is at %02x:%02x:%02x:%02x:%02x:%02x",
          sender_ip.parts[0], sender_ip.parts[1],
          sender_ip.parts[2], sender_ip.parts[3],
          sender_mac.parts[0], sender_mac.parts[1], sender_mac.parts[2],
          sender_mac.parts[3], sender_mac.parts[4], sender_mac.parts[5]);
  } else if (op == ARP_REQUEST) {
    // Only reply when someone is asking for our IP
    if (packet->target_ip_address.value == g_ip.value)
      arp_reply_ipv4(sender_ip, sender_mac);
  }
}

void arp_send_ipv4(ipv4_addr addr) {
  arp_packet_ipv4 arp_packet;
  arp_packet.hardware_type = htons(1);
  arp_packet.protocol_type = htons(ETHERTYPE_IPV4);
  arp_packet.hardware_length = 6;
  arp_packet.protocol_length = 4;
  arp_packet.op_code = htons(ARP_REQUEST);
  e1000_get_mac(&arp_packet.sender_mac_address);
  arp_packet.sender_ip_address = g_ip;
  memset8(arp_packet.target_mac_address.parts, 0x00, 6);
  arp_packet.target_ip_address = addr;
  arp_packet.target_ip_address = addr;

  mac_addr broadcast = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  ethernet_send(broadcast, ETHERTYPE_ARP, &arp_packet, sizeof(arp_packet_ipv4));
}

void arp_table_add(ipv4_addr ip, mac_addr mac) {
  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table[i].ip.value == 0) {
      arp_table[i].ip.value = ip.value;
      arp_table[i].mac = mac;
      return;
    }
  }
}
int arp_table_lookup(ipv4_addr ip, mac_addr *mac_out) {

  if (ip.value == 0xFFFFFFFF) {
    memset8(mac_out->parts, 0xff, 6);
    return 1;
  }

  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    if (arp_table[i].ip.value == ip.value) {
      memcpy8(mac_out->parts, arp_table[i].mac.parts, 6);
      return 1;
    }
  }
  return 0;
}