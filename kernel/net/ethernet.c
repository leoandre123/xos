#include "ethernet.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/arp.h"
#include "net/drivers/network_driver.h"
#include "net/ip.h"
#include "net/net.h"
#include "net/networking.h"
#include "net_types.h"
#include "types.h"

void ethernet_receive(ubyte *data, ushort len, nic *nic) {
  if (len < sizeof(ethernet_frame))
    return;

  ethernet_frame *frame = (ethernet_frame *)data;
  ushort ethertype = ntohs(frame->ethertype);
  ubyte *payload = data + sizeof(ethernet_frame);
  ushort plen = len - sizeof(ethernet_frame);
  switch (ethertype) {
  case ETHERTYPE_ARP:
    arp_receive(payload, plen, nic);
    break;
  case ETHERTYPE_IPV4:
    ip_receive(payload, plen);
    break;
  default:
    // Unknown ethertype — ignore
    break;
  }
}

void ethernet_send(mac_addr dst, ushort ethertype, void *payload, ushort payload_len, net_ops *driver) {
  ushort total = sizeof(ethernet_frame) + payload_len;
  ubyte *buf = kmalloc(total);

  ethernet_frame *frame = (ethernet_frame *)buf;
  driver->get_mac(&frame->src);
  frame->dst = dst;
  frame->ethertype = htons(ethertype);

  memcpy8(buf + sizeof(ethernet_frame), payload, payload_len);
  driver->transmit(buf, total);
  kfree(buf);
}
