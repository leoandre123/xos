#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/arp.h"
#include "net/drivers/network_driver.h"
#include "net/ethernet.h"
#include "net_types.h"
#include "panic.h"
#include "types.h"

#define MAX_PENDING_FRAMES 128

typedef struct {
  ipv4_addr waiting_for;
  net_ops *driver;
  void *frame;
  ushort len;
} pending_frame;

static pending_frame s_pending[MAX_PENDING_FRAMES];

static pending_frame *get_pending_slot() {
  for (int i = 0; i < MAX_PENDING_FRAMES; i++) {
    if (s_pending[i].frame == 0) {
      return &s_pending[i];
    }
  }
  return 0;
}
static void free_pending_slot(pending_frame *slot) { slot->frame = 0; }

void ethernet_driver_send(nic *nic, void *data, ushort len, ipv4_addr gateway) {
  mac_addr dst_mac;
  if (!arp_table_lookup(gateway, &dst_mac)) {
    pending_frame *slot = get_pending_slot();
    slot->len = len;
    slot->waiting_for = gateway;
    slot->driver = nic->driver;
    slot->frame = kmalloc(len);
    memcpy8(slot->frame, data, len);
    arp_send_ipv4(gateway, nic);
    return;
  }
  ethernet_send(dst_mac, ETHERTYPE_IPV4, data, len, nic->driver);
}

void ethernet_driver_flush_pending(ipv4_addr resolved) {
  mac_addr dst_mac;
  if (!arp_table_lookup(resolved, &dst_mac)) {
    panic("Error");
  }
  for (int i = 0; i < MAX_PENDING_FRAMES; i++) {
    pending_frame *frame = &s_pending[i];
    if (frame->len && frame->waiting_for.value == resolved.value) {
      ethernet_send(dst_mac, ETHERTYPE_IPV4, frame->frame, frame->len, frame->driver);
      kfree(frame->frame);
      free_pending_slot(frame);
    }
  }
}