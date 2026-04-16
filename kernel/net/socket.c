#include "socket.h"
#include "io/serial.h"
#include "memory/memutils.h"
#include "net/ip.h"
#include "net/tcp.h"
#include "net/udp.h"
#include "panic.h"
#include "scheduler/scheduler.h"
#include "syscall.h"

static raw_socket g_raw_sockets[MAX_RAW_SOCKETS];
static socket_id g_next_socket_id = 1;

static inline socket_id get_socket_id() {
  return g_next_socket_id++;
}

void socket_init() {
  memset8((ubyte *)&g_raw_sockets, 0, sizeof(g_raw_sockets));
  udp_init();
}

socket_handle socket_tcp_client(ipv4_addr remote_addr, ushort remote_port, ushort local_port) {
  socket_id id = get_socket_id();
  tcp_error err = tcp_active(id, remote_addr, remote_port, local_port);
  if (err) {
    return (socket_handle){.id = 0};
  }
  return (socket_handle){.id = id, .protocol = SOCKET_TCP};
}
socket_handle socket_tcp_server(ushort local_port) {
  socket_id id = get_socket_id();
  tcp_error err = tcp_passive(id, local_port);
  if (err) {
    return (socket_handle){.id = 0};
  }
  return (socket_handle){.id = id, .protocol = SOCKET_TCP};
}

socket_handle socket_udp(ipv4_addr remote_addr, ushort remote_port, ushort local_port) {
  socket_id id = get_socket_id();
  udp_error err = udp_open_socket(id, remote_addr, remote_port, local_port);
  if (err) {
    serial_printf("SOCKET_UDP ERROR %d\n", err);
    return (socket_handle){.id = 0};
  }
  return (socket_handle){.id = id, .protocol = SOCKET_UDP};
}

socket_handle socket_accept(socket_handle handle) {
  if (handle.protocol != SOCKET_TCP)
    panic("INVALID USE");

  socket_id new_id = get_socket_id();

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
  int read_len;
  switch (handle.protocol) {
  case SOCKET_TCP:
    do {
      read_len = tcp_receive(handle.id, buf, len);
      schedule();
    } while (!read_len);
    return read_len;
  case SOCKET_UDP:
    do {
      read_len = udp_receive(handle.id, buf, len);
      schedule();
    } while (!read_len);
    return read_len;
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
  if (queue->packets[queue->tail].len > max_len)
    return 0;

  memcpy8(data, queue->packets[queue->tail].data, queue->packets[queue->tail].len);

  queue->tail = (queue->tail + 1) % SOCKET_QUEUE_SIZE;
  return 1;
}