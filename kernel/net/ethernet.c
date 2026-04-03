#include "ethernet.h"
#include "io/e1000.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/ip.h"
#include "net/net.h"
#include "types.h"

// Forward declarations — upper layers provide these
void arp_receive(ubyte *data, ushort len);

void ethernet_receive(ubyte *data, ushort len) {
  if (len < sizeof(ethernet_frame))
    return;

  ethernet_frame *frame = (ethernet_frame *)data;
  ushort ethertype = ntohs(frame->ethertype);
  ubyte *payload = data + sizeof(ethernet_frame);
  ushort plen = len - sizeof(ethernet_frame);

  switch (ethertype) {
  case ETHERTYPE_ARP:
    arp_receive(payload, plen);
    break;
  case ETHERTYPE_IPV4:
    ip_receive(payload, plen);
    break;
  default:
    // Unknown ethertype — ignore
    break;
  }
}

void ethernet_send(ubyte dst[6], ushort ethertype, void *payload, ushort payload_len) {
  ushort total = sizeof(ethernet_frame) + payload_len;
  ubyte *buf = kmalloc(total);

  ethernet_frame *frame = (ethernet_frame *)buf;
  e1000_get_mac(frame->src);
  for (int i = 0; i < 6; i++)
    frame->dst[i] = dst[i];
  frame->ethertype = htons(ethertype);

  memcpy8(buf + sizeof(ethernet_frame), payload, payload_len);
  e1000_send(buf, total);
  kfree(buf);
}
