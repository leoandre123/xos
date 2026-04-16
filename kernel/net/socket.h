#pragma once
#include "net/ip.h"
#include "net_types.h"
#include "types.h"

#define SOCKET_QUEUE_SIZE 8
#define MAX_RAW_SOCKETS   2

typedef ushort socket_id;

typedef enum : ubyte {
  SOCKET_RAW = 0,
  SOCKET_ICMP = 1,
  SOCKET_TCP = 6,
  SOCKET_UDP = 17,
} socket_protocol;

typedef union {
  struct {
    socket_protocol protocol;
    socket_id id;
  };
  uint value;
} socket_handle;

typedef struct {
  ipv4_header ip_header;
  ushort len;
  ubyte data[IP_MAX_PACKET_SIZE];
} socket_packet;

typedef struct {
  socket_packet packets[SOCKET_QUEUE_SIZE];
  int head, tail;
} socket_queue;

typedef struct {
  socket_id id;
  socket_protocol protocol;
  socket_queue packets;
  ipv4_addr remote_addr;
} raw_socket;

socket_handle socket_tcp_client(ipv4_addr remote_addr, ushort remote_port, ushort local_port);
socket_handle socket_tcp_server(ushort local_port);
socket_handle socket_udp(ipv4_addr remote_addr, ushort remote_port, ushort local_port);
socket_handle socket_icmp();
socket_handle socket_raw();

// int socket_connect(socket_handle handle);
// int socket_listen(socket_handle handle);
socket_handle socket_accept(socket_handle handle);

int socket_recv(socket_handle handle, void *buf, ushort len);
int socket_send(socket_handle handle, void *buf, ushort len);

void socket_close(socket_handle handle);

void socket_init();

void socket_on_data(void *data, ushort data_len);

int socket_queue_write(socket_queue *queue, void *data, ushort len);
int socket_queue_read(socket_queue *queue, void *data, ushort max_len);