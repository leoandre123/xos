#include "ip.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/arp.h"
#include "net/dhcp.h"
#include "net/ethernet.h"
#include "net/icmp.h"
#include "net/net.h"
#include "net/socket.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "net_types.h"
#include "types.h"

typedef struct {
  ipv4_addr ip;
  void *packet;
  ushort packet_len;
  int exists;
} pending_ip_packet;

static pending_ip_packet g_pending_packet[IP_MAX_PENDING_PACKETS];

static ushort
ip_checksum(void *data, ushort len) {
  ushort *ptr = (ushort *)data;
  uint sum = 0;
  while (len > 1) {
    sum += *ptr++;
    len -= 2;
  }
  if (len)
    sum += *(ubyte *)ptr;
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  return ~sum;
}

static void ip_add_pending(ipv4_addr dst_addr, void *packet, ushort packet_len) {
  for (int i = 0; i < IP_MAX_PENDING_PACKETS; i++) {
    if (!g_pending_packet[i].exists) {
      g_pending_packet[i].ip = dst_addr;
      g_pending_packet[i].packet_len = packet_len;
      g_pending_packet[i].packet = packet;
      g_pending_packet[i].exists = 1;
      break;
    }
  }
}

static ipv4_addr next_hop(ipv4_addr dst) {
  // 10.0.2.x → local, send directly
  if (dst.parts[0] == 10 && dst.parts[1] == 0 && dst.parts[2] == 2)
    return dst;
  // everything else → default gateway
  return ipv4(10, 0, 2, 2);
}

void ip_send(ipv4_addr dst_addr, ubyte protocol, void *payload, ushort payload_len) {

  static ushort ip_id = 0;

  ipv4_header header;

  header.version_ihl = (4 << 4) | 5;
  header.dscp_ecn = 0;
  header.total_length = htons(sizeof(ipv4_header) + payload_len);
  header.identification = htons(ip_id++);
  header.flags_fragment_offset = htons(1 << 14);
  header.ttl = 64;
  header.protocol = protocol;
  header.header_checksum = 0;
  header.src_addr = g_ip;
  header.dst_addr = dst_addr;

  header.header_checksum = ip_checksum(&header, sizeof(ipv4_header));

  serial_write("ip_send. g_ip = ");
  serial_write_hex(g_ip.value);
  serial_write_char('\n');

  ushort total = sizeof(ipv4_header) + payload_len;
  void *packet = kmalloc(total);
  memcpy8(packet, (ubyte *)&header, sizeof(ipv4_header));
  memcpy8(((ubyte *)packet) + sizeof(ipv4_header), payload, payload_len);

  ipv4_addr arp_target = next_hop(dst_addr);
  mac_addr mac;
  if (arp_table_lookup(arp_target, &mac)) {
    ethernet_send(mac, ETHERTYPE_IPV4, packet, total);
    kfree(packet);
  } else {
    ip_add_pending(arp_target, packet, total);
    arp_send_ipv4(arp_target);
  }
}

void ip_send_pending(ipv4_addr dst_addr) {
  for (int i = 0; i < IP_MAX_PENDING_PACKETS; i++) {
    if (g_pending_packet[i].exists && (dst_addr.value == g_pending_packet[i].ip.value)) {
      mac_addr mac;
      arp_table_lookup(g_pending_packet[i].ip, &mac);

      ethernet_send(mac, ETHERTYPE_IPV4, g_pending_packet[i].packet, g_pending_packet[i].packet_len);
      kfree(g_pending_packet[i].packet);
      g_pending_packet[i].packet = 0;
      g_pending_packet[i].exists = 0;
    }
  }
}

void ip_receive(void *data, ushort data_len) {
  serial_write_line("IP: received packet");
  if (data_len < sizeof(ipv4_header)) {
    return;
  }

  socket_on_data(data, data_len);
  // ipv4_header *header = (ipv4_header *)data;
  // if (header->protocol == PROTOCOL_UDP) {
  //   udp_receive(((ubyte *)data) + sizeof(ipv4_header), data_len - sizeof(ipv4_header));
  // } else if (header->protocol == PROTOCOL_TCP) {
  //   tcp_on_data(header->src_addr, ((ubyte *)data) + sizeof(ipv4_header), data_len - sizeof(ipv4_header));
  // } else if (header->protocol == PROTOCOL_ICMP) {
  //   icmp_receive(header->src_addr, ((ubyte *)data) + sizeof(ipv4_header), data_len - sizeof(ipv4_header));
  // }
}