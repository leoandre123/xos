#pragma once

#include "net/socket.h"
#include "net_types.h"
#include "types.h"
#include "utils/ring_buf.h"

#define TCP_MAX_LISTENERS   4
#define TCP_MAX_CONNECTIONS 4
#define TCP_BACKLOG_SIZE    4

typedef enum {
  TCP_OK,
  TCP_PORT_RESERVED,
  TCP_PORT_USED,
  TCP_SOCKET_LIMIT_EXCEEDED,
  TCP_NO_PORT_PROVIDED,
  TCP_INVALID_ID
} tcp_error;

typedef struct {
  ushort src_port;
  ushort dst_port;
  uint seq_num;
  uint ack_num;
  ubyte data_offset;
  ubyte flags;
  ushort window;
  ushort checksum;
  ushort urgent_ptr;
} __attribute__((__packed__)) tcp_header;

typedef enum {
  TCP_CLOSED,
  TCP_LISTEN,
  TCP_SYN_SENT,
  TCP_SYN_RECEIVED,
  TCP_ESTABLISHED,
  TCP_FIN_WAIT_1,
  TCP_FIN_WAIT_2,
  TCP_CLOSE_WAIT,
  TCP_CLOSING,
  TCP_TIME_WAIT,
  TCP_LAST_ACK,
} tcp_state;

typedef struct {
  socket_id id;
  int claimed;
  tcp_state state;
  ipv4_addr local_ip;
  ipv4_addr remote_ip;
  ushort local_port;
  ushort remote_port;

  uint seq;         // our next sequence number to send
  uint ack;         // next byte we expect from them
  uint initial_seq; // ISN, used to convert ack numbers to buffer offsets

  /*
    [delivered|undelivered|free]
               ^           ^
               tail        ack and head
  */
  ubyte recv_buf[4096];
  uint recv_head, recv_tail;

  /*
    [acked|unacked|unsent|free]
           ^       ^      ^
           tail    send_  head
  */
  ubyte send_buf[4096];
  uint send_head, send_tail;
} tcp_active_socket;

RING_BUF(backlog, socket_id, TCP_BACKLOG_SIZE);

typedef struct {
  socket_id id;
  ushort local_port;
  // socket_id backlog[TCP_BACKLOG_SIZE];
  // uint backlog_head, backlog_tail;
  backlog backlog;
  // bool is_listening;
} tcp_passive_socket;

/************
 * USER API *
 ************/

/* Functions that generate connections */
tcp_error tcp_active(socket_id id, ipv4_addr remote_addr, ushort remote_port, ushort local_port);
tcp_error tcp_passive(socket_id id, ushort port);
bool tcp_accept(socket_id new_id, socket_id id);

// void tcp_init_connection(socket_id id);
int tcp_terminate_connection(uint conn_id);
int tcp_send(uint conn_id, void *data, ushort data_len);
int tcp_receive(uint conn_id, void *data, ushort data_len);

/**************
 * SYSTEM API *
 **************/
void tcp_on_data(ipv4_addr src_addr, void *data, ushort data_len);
void tcp_send_buffered_data();
