#pragma once
#include "net/ip.h"
#include "net_types.h"
#include "scheduler/task.h"
#include "types.h"

#define SOCKET_QUEUE_SIZE 8
#define MAX_RAW_SOCKETS   2

typedef ushort socket_id;

typedef union {
  struct {
    socket_protocol protocol;
    socket_id id;
  };
  uint value;
  ulong value_ulong;
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

socket_handle socket(socket_protocol protocol);       // Create socket
void socket_close(socket_handle handle);              // Closes socket
int socket_bind(socket_handle h, socket_addr *local); // Bind to local interface
int socket_bind_nic(socket_handle h, int nic_id);
int socket_listen(socket_handle h);                       // Listens to local interface (tcp only)
socket_handle socket_accept(socket_handle h);             // Return socket to incoming connection
int socket_connect(socket_handle h, socket_addr *remote); // Connects to remote endpoint

int socket_recv(socket_handle handle, void *buf, ushort len);
int socket_send(socket_handle handle, void *buf, ushort len);

/* SYSTEMS API */
void socket_init();
void socket_on_data(void *data, ushort data_len);
int socket_queue_write(socket_queue *queue, void *data, ushort len);
int socket_queue_read(socket_queue *queue, void *data, ushort max_len);
bool socket_set_receiver(socket_handle handle, task *t);