#include "icmp.h"
#include "net/ip.h"
#include "net_types.h"

static ushort checksum(void *data, uint len) {
  ushort *p = (ushort *)data;
  uint sum = 0;

  while (len > 1) {
    sum += *p++;
    len -= 2;
  }
  if (len) // odd byte
    sum += *(ubyte *)p;

  // fold 32-bit sum into 16 bits
  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);

  return (ushort)~sum;
}

void icmp_send_ping(ipv4_addr dst_addr) {
  icmp_header header = {.type = ICMP_ECHO_REQUEST, .code = 0, .checksum = 0, .rest = 0};
  header.checksum = checksum(&header, sizeof(icmp_header));
  ip_send(dst_addr, PROTOCOL_ICMP, &header, sizeof(icmp_header), (ip_send_opts){});
}
void icmp_receive(ipv4_addr src_addr, void *data, ushort data_len) {
  if (data_len < sizeof(icmp_header)) {
    return;
  }
  icmp_header *header = data;

  if (header->type == ICMP_ECHO_REQUEST) {
    icmp_header rheader = {.type = ICMP_ECHO_REPLY, .code = 0, .checksum = 0, .rest = 0};
    rheader.checksum = checksum(&rheader, sizeof(icmp_header));
    ip_send(src_addr, PROTOCOL_ICMP, &rheader, sizeof(icmp_header), (ip_send_opts){});
  }
}
