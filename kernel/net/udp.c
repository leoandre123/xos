#include "udp.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/dhcp.h"
#include "net/ip.h"
#include "net/net.h"
#include "net/socket.h"

typedef struct {
  socket_id id;
  ushort remote_port;
  ushort local_port;
  ipv4_addr remote_addr;
  socket_queue packets;
} udp_socket;

static udp_socket g_sockets[UDP_MAX_SOCKETS];
static const ushort reserved_ports[] = {
    UDP_PORT_DNS,
    UDP_PORT_DHCP_CLIENT,
    0,
};

static inline int is_reserved(ushort port) {
  for (int i = 0; reserved_ports[i] != 0; i++)
    if (reserved_ports[i] == port)
      return 1;
  return 0;
}

static inline int is_port_used(ushort port) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (g_sockets[i].id && g_sockets[i].local_port == port)
      return 1;
  }
  return 0;
}

static inline udp_socket *get_socket(socket_id id) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (g_sockets[i].id == id)
      return &g_sockets[i];
  }
  return 0;
}

void udp_init() {
  memset8((ubyte *)&g_sockets, 0, sizeof(g_sockets));
}

udp_error udp_open_socket(socket_id id, ipv4_addr remote_addr, ushort remote_port, ushort local_port) {
  if (is_reserved(local_port)) {
    return UDP_PORT_RESERVED;
  }
  if (is_port_used(local_port)) {
    return UDP_PORT_USED;
  }

  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (g_sockets[i].id == 0) {
      g_sockets[i].id = id;
      g_sockets[i].local_port = local_port;
      g_sockets[i].remote_port = remote_port;
      g_sockets[i].remote_addr = remote_addr;
      return UDP_OK;
    }
  }
  return UDP_SOCKET_LIMIT_EXCEEDED;
}
void udp_close_socket(socket_id id) {
  for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
    if (g_sockets[i].id == id) {
      g_sockets[i].id = 0;
      return;
    }
  }
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
  ip_send(dst_addr, PROTOCOL_UDP, packet, total_len);
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
  ip_send(socket->remote_addr, PROTOCOL_UDP, packet, total_len);
  kfree(packet);
  return payload_len;
}

int udp_receive(socket_id id, void *data, ushort data_len) {
  udp_socket *socket = get_socket(id);
  if (!socket)
    return 0;

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

  if (dst_port == UDP_PORT_DHCP_CLIENT) {
    dhcp_receive(payload, payload_len);
  } else {
    for (int i = 0; i < UDP_MAX_SOCKETS; i++) {
      if (g_sockets[i].id && g_sockets[i].local_port == dst_port) {
        socket_queue_write(&g_sockets[i].packets, payload, payload_len);
        break;
      }
    }
  }
}