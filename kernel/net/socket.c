#include "socket.h"
#include "io/logging.h"
#include "io/serial.h"
#include "memory/memutils.h"
#include "net/ip.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "panic.h"
#include "scheduler/scheduler.h"
#include "syscall.h"
#include "types.h"
#include "utils/math.h"

static raw_socket g_raw_sockets[MAX_RAW_SOCKETS];
static socket_id g_next_socket_id = 1;

static inline socket_id generate_socket_id() {
  return g_next_socket_id++;
}

void socket_init() {
  memset8((ubyte *)&g_raw_sockets, 0, sizeof(g_raw_sockets));
  udp_init();
}

socket_handle socket(socket_protocol protocol) {
  socket_id id = generate_socket_id();
  int err = 0;
  switch (protocol) {
  case SOCKET_RAW:
    break;
  case SOCKET_ICMP:
    break;
  case SOCKET_TCP:
    err = tcp_socket(id);
    break;
  case SOCKET_UDP:
    err = udp_create_socket(id);
    break;
  }
  if (err) {
    return (socket_handle){.id = 0};
  }
  return (socket_handle){.id = id, .protocol = protocol};
}

int socket_bind(socket_handle h, socket_addr *local) {
  int err = 0;
  switch (h.protocol) {
  case SOCKET_RAW:
    break;
  case SOCKET_ICMP:
    break;
  case SOCKET_TCP:
    err = tcp_bind(h.id, local);
    break;
  case SOCKET_UDP:
    err = udp_bind_socket(h.id, local);
    break;
  }
  return err;
}

int socket_bind_nic(socket_handle h, int nic_id) {
  int err = 0;
  switch (h.protocol) {
  case SOCKET_UDP:
    err = udp_bind_nic(h.id, nic_id);
    break;
  default:
    panic("Not implemented");
  }
  return err;
}

int socket_listen(socket_handle h) {
  int err = 0;
  switch (h.protocol) {
  case SOCKET_RAW:
    break;
  case SOCKET_ICMP:
    break;
  case SOCKET_TCP:
    err = tcp_listen(h.id);
    break;
  case SOCKET_UDP:
    err = udp_listen_socket(h.id);
    break;
  }
  return err;
}

int socket_connect(socket_handle h, socket_addr *remote) {
  int err = 0;
  switch (h.protocol) {
  case SOCKET_RAW:
    break;
  case SOCKET_ICMP:
    break;
  case SOCKET_TCP:
    err = tcp_connect(h.id, remote);
    break;
  case SOCKET_UDP:
    err = udp_connect(h.id, remote);
    break;
  }
  return err;
}

socket_handle socket_accept(socket_handle handle) {
  if (handle.protocol != SOCKET_TCP)
    panic("INVALID USE");

  socket_id new_id = generate_socket_id();

  while (!tcp_accept(new_id, handle.id)) {
    schedule();
  }

  return (socket_handle){.id = new_id, .protocol = SOCKET_TCP};
}

void socket_close(socket_handle handle) {
  switch (handle.protocol) {
  case SOCKET_RAW:
    panic("not implemented!");
  case SOCKET_ICMP:
    panic("not implemented!");
  case SOCKET_TCP:
    tcp_terminate_connection(handle.id);
    break;
  case SOCKET_UDP:
    udp_close_socket(handle.id);
    break;
  }
}

void socket_on_data(void *data, ushort data_len) {
  if (data_len < sizeof(ipv4_header)) {
    return;
  }
  ipv4_header *header = (ipv4_header *)data;
  // Use IP total_length, not the raw frame length, to strip Ethernet padding
  ushort ip_total = ntohs(header->total_length);
  if (ip_total < sizeof(ipv4_header) || ip_total > data_len)
    return;
  void *payload = (ubyte *)data + sizeof(ipv4_header);
  int payload_len = ip_total - sizeof(ipv4_header);

  switch (header->protocol) {
  case PROTOCOL_TCP:
    tcp_on_data(header->src_addr, payload, payload_len);
    break;
  case PROTOCOL_UDP:
    udp_on_data(payload, payload_len);
    break;
  case PROTOCOL_ICMP:
    break;
  default:
    break;
  }
}

int socket_recv(socket_handle handle, void *buf, ushort len) {
  switch (handle.protocol) {
  case SOCKET_TCP:
    return tcp_receive(handle.id, buf, len);
  case SOCKET_UDP:
    return udp_receive(handle.id, buf, len);
  default:
    panic("Not implemented");
    return 0;
  }
}

int socket_send(socket_handle handle, void *buf, ushort len) {
  switch (handle.protocol) {
  case SOCKET_TCP:
    tcp_send(handle.id, buf, len);
    break;
  case SOCKET_UDP: {
    int sent = udp_send(handle.id, buf, len);
    if (!sent)
      serial_printf("ERROR ERROR ERROR");
    break;
  }
  default:
    panic("Not implemented");
  }
  return 1;
}

int socket_queue_write(socket_queue *queue, void *data, ushort len) {
  if ((queue->head + 1) % SOCKET_QUEUE_SIZE == queue->tail) {
    // buffer is full, avoid overflow
    klogf(LOG_WARNING, "Packet dropped for: buffer full");
    return 0;
  }
  queue->packets[queue->head].len = len;
  memcpy8(queue->packets[queue->head].data, data, len);
  queue->head = (queue->head + 1) % SOCKET_QUEUE_SIZE;
  return 1;
}
int socket_queue_read(socket_queue *queue, void *data, ushort max_len) {
  if (queue->head == queue->tail) {
    // buffer is empty
    return 0;
  }
  ushort copy_len = MIN(queue->packets[queue->tail].len, max_len);

  memcpy8(data, queue->packets[queue->tail].data, copy_len);

  queue->tail = (queue->tail + 1) % SOCKET_QUEUE_SIZE;
  return 1;
}

bool socket_set_receiver(socket_handle handle, task *t) {
  switch (handle.protocol) {
  case SOCKET_TCP:
    return tcp_set_receiver(handle.id, t);
  case SOCKET_UDP:
    return udp_set_receiver(handle.id, t);
  default:
    panic("Not implemented");
  }
}