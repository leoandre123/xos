#include "udp.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/dhcp.h"
#include "net/ip.h"
#include "net/net.h"

void udp_send(ipv4_addr dst_addr, ushort src_port, ushort dst_port, void *payload, ushort payload_len) {

  ushort total_len = sizeof(udp_header) + payload_len;

  udp_header header;
  header.src_port = htons(src_port);
  header.dst_port = htons(dst_port);
  header.length = htons(total_len);
  header.checksum = 0; // optional in IPv4

  void *packet = kmalloc(total_len);
  memcpy8(packet, (ubyte *)&header, sizeof(udp_header));
  memcpy8(((ubyte *)packet) + sizeof(udp_header), payload, payload_len);
  ip_send(dst_addr, PROTOCOL_UDP, packet, total_len);
  kfree(packet);
}

void udp_receive(void *data, ushort data_len) {
  if (data_len < sizeof(udp_header)) {
    return;
  }

  udp_header *header = data;

  ushort dst_port = ntohs(header->dst_port);

  serial_printf("UDP: received packet with length %1d bytes to port %1d\n", data_len, dst_port);

  if (dst_port == 68) {
    dhcp_receive(((ubyte *)data) + sizeof(udp_header), data_len - +sizeof(udp_header));
  }
}