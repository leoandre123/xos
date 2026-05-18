#pragma once

#include "net/socket.h"
#include "types.h"

#define UDP_MAX_SOCKETS 16

#define UDP_PORT_DHCP_CLIENT 68
#define UDP_PORT_DNS         53

typedef enum {
  UDP_OK,
  UDP_PORT_RESERVED,
  UDP_PORT_USED,
  UDP_SOCKET_LIMIT_EXCEEDED,
} udp_error;

typedef struct {
  ushort src_port;
  ushort dst_port;
  ushort length;
  ushort checksum;
} udp_header;

void udp_init();

udp_error udp_open_socket(socket_id id, ipv4_addr remote_addr, ushort remote_port, ushort local_port);
void udp_close_socket(socket_id id);

void udp_send_old(ipv4_addr dst_addr, ushort src_port, ushort dst_port, void *payload, ushort payload_len);
int udp_send(socket_id id, void *payload, ushort payload_len);
int udp_receive(socket_id id, void *data, ushort data_len);
void udp_on_data(void *data, ushort data_len);