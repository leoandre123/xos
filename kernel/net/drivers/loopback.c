#include "loopback.h"
#include "memory/memutils.h"
#include "net/ip.h"
#include "net/networking.h"
#include "net_types.h"
#include "utils/ring_buf.h"

#define FRAME_SIZE 1022
#define BUFFER_LEN 32

typedef struct {
  ubyte data[FRAME_SIZE];
  ushort len;
} loopback_frame;

RING_BUF(loopback_buffer, loopback_frame, BUFFER_LEN);
static loopback_buffer s_buffer;

static void loopback_poll(nic *nic) {
  loopback_frame out;
  if (!loopback_buffer_read(&s_buffer, &out))
    return;
  ip_receive(&out.data[0], out.len);
}
static void loopback_send(nic *nic, void *data, ushort len, ipv4_addr gateway) {
  loopback_frame frame;
  memcpy8((ubyte *)&frame, data, len);
  loopback_buffer_write(&s_buffer, &frame);
}

net_ops g_loopback_ops = {
    .init = 0,
    .poll = loopback_poll,
    .send = loopback_send,
    .get_mac = 0,
    .transmit = 0};