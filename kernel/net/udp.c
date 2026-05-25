#include "udp.h"
#include "io/logging.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/ip.h"
#include "net/net.h"
#include "net/socket.h"
#include "net_types.h"
#include "scheduler/scheduler.h"
#include "scheduler/task.h"
#include "utils/utils.h"

typedef struct {
  socket_id id;
  ushort remote_port;
  ushort local_port;
  ipv4_addr remote_addr;
  ipv4_addr local_addr;
  int nic_id;
  bool is_listenting;
  task *receiver;
  socket_queue packets;
} udp_socket;

static udp_socket s_sockets[UDP_MAX_SOCKETS];
static const ushort s_reserved_ports[] = {UDP_PORT_DNS};

static inline bool is_reserved(ushort port) {
  for (int i = 0; i < lenof(s_reserved_ports); i++)
    if (s_reserved_ports[i] == port)
      return true;
  return false;
}

static inline bool is_port_used(ushort port) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (s_sockets[i].id && s_sockets[i].local_port == port)
      return true;
  }
  return false;
}
static inline udp_socket *get_socket_for_port(ushort port) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (s_sockets[i].id && s_sockets[i].local_port == port)
      return &s_sockets[i];
  }
  return 0;
}
static inline udp_socket *find_free_socket() {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (s_sockets[i].id == 0) {
      return &s_sockets[i];
    }
  }
  return 0;
}

static inline udp_socket *get_socket(socket_id id) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (s_sockets[i].id == id)
      return &s_sockets[i];
  }
  return 0;
}

void udp_init() {
  memset8((ubyte *)&s_sockets, 0, sizeof(s_sockets));
}

int udp_create_socket(socket_id id) {
  udp_socket *s = find_free_socket();
  if (!s)
    return UDP_SOCKET_LIMIT_EXCEEDED;
  s->id = id;
  s->local_port = 0;
  s->remote_port = 0;
  s->remote_addr = IP(0, 0, 0, 0);
  s->local_addr = IP(0, 0, 0, 0);
  s->is_listenting = false;
  s->nic_id = 0;
  s->receiver = 0;
  return UDP_OK;
}
int udp_bind_socket(socket_id id, socket_addr *addr) {
  udp_socket *s = get_socket(id);
  if (!s)
    return UDP_SOCKET_NOT_FOUND;

  if (is_reserved(addr->udp_addr.port)) {
    return UDP_PORT_RESERVED;
  }
  if (is_port_used(addr->udp_addr.port)) {
    return UDP_PORT_USED;
  }

  s->local_port = addr->udp_addr.port;
  s->local_addr = addr->udp_addr.addr;
  return UDP_OK;
}
int udp_bind_nic(socket_id id, int nic_id) {
  udp_socket *s = get_socket(id);
  if (!s)
    return UDP_SOCKET_NOT_FOUND;

  s->nic_id = nic_id;
  return UDP_OK;
}

int udp_listen_socket(socket_id id) {
  udp_socket *s = get_socket(id);
  if (!s)
    return UDP_SOCKET_NOT_FOUND;
  s->is_listenting = true;
  return UDP_OK;
}

void udp_close_socket(socket_id id) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (s_sockets[i].id == id) {
      s_sockets[i].id = 0;
      return;
    }
  }
}

int udp_connect(socket_id id, socket_addr *remote) {
  udp_socket *s = get_socket(id);
  if (!s)
    return UDP_SOCKET_NOT_FOUND;

  s->is_listenting = true;
  s->remote_port = remote->udp_addr.port;
  s->remote_addr = remote->udp_addr.addr;
  return UDP_OK;
}

void udp_send_old(ipv4_addr dst_addr, ushort src_port, ushort dst_port, void *payload, ushort payload_len) {
  ushort total_len = sizeof(udp_header) + payload_len;
  udp_header header;
  header.src_port = htons(src_port);
  header.dst_port = htons(dst_port);
  header.length = htons(total_len);
  header.checksum = 0;

  void *packet = kmalloc(total_len);
  memcpy8(packet, (ubyte *)&header, sizeof(udp_header));
  memcpy8(((ubyte *)packet) + sizeof(udp_header), payload, payload_len);
  ip_send(dst_addr, PROTOCOL_UDP, packet, total_len, (ip_send_opts){});
  kfree(packet);
}

int udp_send(socket_id id, void *payload, ushort payload_len) {
  udp_socket *socket = get_socket(id);
  if (!socket)
    return 0;

  ushort total_len = sizeof(udp_header) + payload_len;
  udp_header header;
  header.src_port = htons(socket->local_port);
  header.dst_port = htons(socket->remote_port);
  header.length = htons(total_len);
  header.checksum = 0; // optional in IPv4

  void *packet = kmalloc(total_len);
  memcpy8(packet, (ubyte *)&header, sizeof(udp_header));
  memcpy8(((ubyte *)packet) + sizeof(udp_header), payload, payload_len);
  ip_send(socket->remote_addr, PROTOCOL_UDP, packet, total_len, (ip_send_opts){.src_addr = socket->local_addr, .nic_id = socket->nic_id});
  kfree(packet);
  return payload_len;
}

int udp_receive(socket_id id, void *data, ushort data_len) {
  // klogf(LOG_TRACE, "Udp receive: %d", id);
  udp_socket *socket = get_socket(id);
  if (!socket) {
    klogf(LOG_TRACE, "No socket found");
    return 0;
  }

  // klogf(LOG_TRACE, "Head: %d", socket->packets.head);

  return socket_queue_read(&socket->packets, data, data_len);
}

void udp_on_data(void *data, ushort data_len) {
  if (data_len < sizeof(udp_header)) {
    return;
  }
  udp_header *header = data;
  ushort dst_port = ntohs(header->dst_port);
  void *payload = ((ubyte *)data) + sizeof(udp_header);
  int payload_len = data_len - sizeof(udp_header);

  serial_printf("UDP: received packet with length %1d bytes to port %1d\n", data_len, dst_port);

  udp_socket *s = get_socket_for_port(dst_port);

  if (!s || !s->is_listenting) {
    serial_printf("Dropped UDP packet: not listerner");
    return;
  }
  socket_queue_write(&s->packets, payload, payload_len);
  klogf(LOG_TRACE, "Data written to socket: %d", s->id);

  if (s->receiver) {
    task_set_ready(s->receiver);
    s->receiver = 0;
  }
}

bool udp_set_receiver(socket_id id, task *t) {
  udp_socket *s = get_socket(id);
  if (!s)
    return false;

  if (s->receiver)
    return false;

  s->receiver = t;
  return true;
}