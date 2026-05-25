#include "tcp.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "net/ip.h"
#include "net/net.h"
#include "net/socket.h"
#include "net_types.h"
#include "types.h"
#include "utils/random.h"

#define TCP_FIN 1
#define TCP_SYN (1 << 1)
#define TCP_RST (1 << 2)
#define TCP_PSH (1 << 3)
#define TCP_ACK (1 << 4)
#define TCP_URG (1 << 5)
#define TCP_ECE (1 << 6)
#define TCP_CWR (1 << 7)

static tcp_passive_socket g_passive_sockets[TCP_MAX_LISTENERS];
static tcp_active_socket g_active_sockets[TCP_MAX_CONNECTIONS];

static tcp_active_socket *resolve_conn(ipv4_addr local_addr, ipv4_addr remote_addr,
                                       ushort local_port, ushort remote_port) {
  for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
    if (g_active_sockets[i].claimed &&
        g_active_sockets[i].local_ip.value == local_addr.value &&
        g_active_sockets[i].remote_ip.value == remote_addr.value &&
        g_active_sockets[i].local_port == local_port &&
        g_active_sockets[i].remote_port == remote_port) {
      return &g_active_sockets[i];
    }
  }
  return 0;
}
static tcp_passive_socket *resolve_passive(ushort local_port) {
  for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
    if (g_passive_sockets[i].id && g_passive_sockets[i].local_port == local_port) {
      return &g_passive_sockets[i];
    }
  }
  return 0;
}

static tcp_active_socket *get_conn(uint conn_id) {
  for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
    if (g_active_sockets[i].id == conn_id) {
      return &g_active_sockets[i];
    }
  }
  return 0;
}

static tcp_passive_socket *get_passive(socket_id id) {
  for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
    if (g_passive_sockets[i].id == id) {
      return &g_passive_sockets[i];
    }
  }
  return 0;
}

static inline tcp_active_socket *create_active(socket_id id, ipv4_addr local_addr, ipv4_addr remote_addr,
                                               ushort local_port, ushort remote_port) {
  for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
    if (!g_active_sockets[i].claimed) {
      memset8((ubyte *)&g_active_sockets[i], 0, sizeof(tcp_active_socket));
      g_active_sockets[i].local_ip.value = local_addr.value;
      g_active_sockets[i].remote_ip.value = remote_addr.value;
      g_active_sockets[i].local_port = local_port;
      g_active_sockets[i].remote_port = remote_port;
      g_active_sockets[i].seq = rand32();
      g_active_sockets[i].initial_seq = g_active_sockets[i].seq;
      g_active_sockets[i].claimed = true;
      g_active_sockets[i].id = id;
      return &g_active_sockets[i];
    }
  }
  return 0;
}

static inline tcp_passive_socket *create_passive(socket_id id, ushort local_port) {
  for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
    if (g_passive_sockets[i].id == 0) {
      g_passive_sockets[i].id = id;
      g_passive_sockets[i].local_port = local_port;
      g_passive_sockets[i].backlog.head = 0;
      g_passive_sockets[i].backlog.tail = 0;
      return &g_passive_sockets[i];
    }
  }
  return 0;
}

int tcp_socket(socket_id id) {}
int tcp_bind(socket_id, socket_addr *addr) {}
int tcp_listen(socket_id id) {}
int tcp_connect(socket_id id, socket_addr *remote) {}

static inline void create_header(tcp_active_socket *conn, ubyte flags, tcp_header *header_out) {
  header_out->src_port = htons(conn->local_port);
  header_out->dst_port = htons(conn->remote_port);
  header_out->flags = flags;
  header_out->data_offset = 5 << 4;
  header_out->seq_num = htonl(conn->seq);
  header_out->ack_num = htonl(conn->ack);
  header_out->window = htons(8192);
  header_out->checksum = 0;
  header_out->urgent_ptr = 0;
}

static ushort tcp_checksum(tcp_active_socket *conn, void *segment, ushort segment_len) {
  ipv4_addr src = conn->local_ip; // conn->local_ip.value ? conn->local_ip : g_ip;
  uint sum = 0;

  // Pseudo-header: build explicit big-endian 16-bit words from byte arrays
  sum += ((uint)src.parts[0] << 8) | src.parts[1];
  sum += ((uint)src.parts[2] << 8) | src.parts[3];
  sum += ((uint)conn->remote_ip.parts[0] << 8) | conn->remote_ip.parts[1];
  sum += ((uint)conn->remote_ip.parts[2] << 8) | conn->remote_ip.parts[3];
  sum += PROTOCOL_TCP;
  sum += segment_len;

  // TCP segment bytes as big-endian 16-bit words
  ubyte *b = (ubyte *)segment;
  for (ushort i = 0; i + 1 < segment_len; i += 2)
    sum += ((uint)b[i] << 8) | b[i + 1];
  if (segment_len & 1)
    sum += (uint)b[segment_len - 1] << 8;

  while (sum >> 16)
    sum = (sum & 0xFFFF) + (sum >> 16);
  return htons((ushort)~sum);
}

static void send_header(tcp_active_socket *conn, ubyte flags) {
  tcp_header header;
  create_header(conn, flags, &header);
  if (flags & (TCP_SYN | TCP_FIN))
    conn->seq++;

  header.checksum = tcp_checksum(conn, &header, sizeof(tcp_header));
  ip_send(conn->remote_ip, PROTOCOL_TCP, &header, sizeof(tcp_header), (ip_send_opts){});
}

static inline int is_port_used(ushort port) {
  for (int i = 0; i < TCP_MAX_LISTENERS; i++) {
    if (g_passive_sockets[i].id && g_passive_sockets[i].local_port == port)
      return 1;
  }
  for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
    if (g_active_sockets[i].id && g_active_sockets[i].local_port == port)
      return 1;
  }
  return 0;
}

static inline ushort get_ephemeral_port() {
  static ushort port = 49152;
  if (port == 0)
    port = 49152;
  return port++;
}

tcp_error tcp_active(socket_id id, ipv4_addr remote_addr, ushort remote_port, ushort local_port) {
  if (remote_port == 0) {
    return TCP_NO_PORT_PROVIDED;
  }
  if (local_port == 0) {
    local_port = get_ephemeral_port();
  } else if (is_port_used(local_port)) {
    return TCP_PORT_USED;
  }

  tcp_active_socket *socket = create_active(id, (ipv4_addr)0u, remote_addr, local_port, remote_port);
  if (!socket) {
    return TCP_SOCKET_LIMIT_EXCEEDED;
  }
  socket->state = TCP_SYN_SENT;
  send_header(socket, TCP_SYN);
  return TCP_OK;
}
tcp_error tcp_passive(socket_id id, ushort port) {
  if (port == 0) {
    return TCP_NO_PORT_PROVIDED;
  }
  if (is_port_used(port)) {
    return TCP_PORT_USED;
  }

  tcp_passive_socket *socket = create_passive(id, port);
  if (!socket) {
    return TCP_SOCKET_LIMIT_EXCEEDED;
  }

  return TCP_OK;
}
bool tcp_accept(socket_id new_id, socket_id id) {
  tcp_passive_socket *listener = get_passive(id);
  if (!listener)
    return false;

  socket_id conn_id = 0;
  if (!backlog_read(&listener->backlog, &conn_id))
    return false;
  if (!conn_id)
    return false;

  tcp_active_socket *socket = get_conn(conn_id);
  if (!socket)
    return false;

  socket->id = new_id;
  return true;
}

int tcp_terminate_connection(uint conn_id) {
  tcp_active_socket *conn = get_conn(conn_id);
  if (!conn) {
    return 0;
  }

  conn->state = TCP_FIN_WAIT_1;
  send_header(conn, TCP_FIN);
  return 1;
}

void tcp_on_data(ipv4_addr src_addr, void *data, ushort data_len) {
  if (data_len < sizeof(tcp_header)) {
    return;
  }
  tcp_header *header = (tcp_header *)data;
  ushort header_len = (header->data_offset >> 4) * 4;
  ushort payload_len = data_len - header_len;
  ubyte *payload = (ubyte *)data + header_len;

  ushort dst_port = ntohs(header->dst_port);
  ushort src_port = ntohs(header->src_port);

  tcp_active_socket *conn = 0;
  conn = resolve_conn((ipv4_addr)0u, src_addr, dst_port, src_port);
  serial_printf("TCP: received packet: %d\n", conn);
  // INCOMING CONNECTION
  if (!conn) {
    tcp_passive_socket *listener = resolve_passive(dst_port);
    if (!listener || backlog_full(&listener->backlog))
      return;
    if (header->flags != TCP_SYN)
      return;

    socket_id conn_id = rand16();
    conn = create_active(conn_id, (ipv4_addr)0u, src_addr, dst_port, src_port);
    if (!conn) {
      return;
    }

    conn->state = TCP_SYN_RECEIVED;
    conn->ack = ntohl(header->seq_num) + 1;
    send_header(conn, TCP_SYN | TCP_ACK);
    return;
  }

  if (header->flags & TCP_RST) {
    conn->state = TCP_CLOSED;
    conn->claimed = false;
    conn->id = 0;
    return;
  }

  // In SYN_SENT we haven't seen their ISN yet; initialize ack from the incoming seq
  if (conn->state == TCP_SYN_SENT) {
    conn->ack = ntohl(header->seq_num);
  } else if (ntohl(header->seq_num) != conn->ack) {
    serial_printf("Incorrect seq num: %d != %d\n", ntohl(header->seq_num), conn->ack);
    return;
  }

  if (header->flags & (TCP_SYN | TCP_FIN))
    conn->ack++;
  conn->ack += payload_len;

  uint acked_offset = ntohl(header->ack_num) - conn->initial_seq - 1;
  if (acked_offset > conn->send_tail) {
    conn->send_tail = acked_offset;
  }

  serial_printf("State: %d\n", conn->state);

  switch (conn->state) {
  case TCP_CLOSED:
    return;
  case TCP_LISTEN:
    return;
  case TCP_SYN_SENT:
    if ((header->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
      conn->state = TCP_ESTABLISHED;
      send_header(conn, TCP_ACK);
      tcp_send_buffered_data();
      break;
    } else {
      send_header(conn, TCP_RST);
      return;
    }
  case TCP_SYN_RECEIVED:
    if ((header->flags & (TCP_ACK))) {
      tcp_passive_socket *listener = resolve_passive(dst_port);
      if (!listener)
        return; // missing
      backlog_write(&listener->backlog, &conn->id);
      conn->state = TCP_ESTABLISHED;
      tcp_send_buffered_data();
      break;
    } else {
      send_header(conn, TCP_RST);
      return;
    }
  case TCP_ESTABLISHED:
    if (payload_len > 0) {
      serial_printf("TCP: payload received: %d", payload_len);
      for (ushort i = 0; i < payload_len; i++)
        conn->recv_buf[conn->recv_head++ % sizeof(conn->recv_buf)] = payload[i];
      send_header(conn, TCP_ACK);
    }
    if (header->flags & TCP_FIN) {
      conn->state = TCP_LAST_ACK;
      send_header(conn, TCP_ACK | TCP_FIN);
    }
    break;
  case TCP_FIN_WAIT_1:
    if (header->flags & TCP_ACK) {
      conn->state = TCP_FIN_WAIT_2;
    }
    if (header->flags & TCP_FIN) {
      conn->state = TCP_TIME_WAIT;
      send_header(conn, TCP_ACK);
    }
    break;
  case TCP_FIN_WAIT_2:
    if (header->flags & TCP_FIN) {
      conn->state = TCP_TIME_WAIT;
      send_header(conn, TCP_ACK);
    }
    break;
  case TCP_CLOSE_WAIT:
  case TCP_CLOSING:
  case TCP_TIME_WAIT:
  case TCP_LAST_ACK:
    if (header->flags & TCP_ACK) {
      conn->state = TCP_CLOSED;
      conn->claimed = false;
      conn->id = 0;
    }
    break;
  }
}

void tcp_send_buffered_data() {
  for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
    if (g_active_sockets[i].state == TCP_ESTABLISHED) {
      tcp_active_socket *conn = &g_active_sockets[i];
      uint payload_len = conn->send_head - conn->send_tail;
      if (payload_len == 0)
        continue;
      uint data_len = payload_len + sizeof(tcp_header);

      void *data = kmalloc(data_len);
      create_header(conn, TCP_ACK, data);
      for (ushort i = 0; i < payload_len; i++)
        ((ubyte *)data)[sizeof(tcp_header) + i] = conn->send_buf[conn->send_tail++ % sizeof(conn->send_buf)];
      ((tcp_header *)data)->checksum = tcp_checksum(conn, data, data_len);
      ip_send(conn->remote_ip, PROTOCOL_TCP, data, data_len, (ip_send_opts){});
      kfree(data);

      conn->seq += payload_len;
    }
  }
}

int tcp_send(uint conn_id, void *data, ushort data_len) {
  if (data_len == 0)
    return 0;
  tcp_active_socket *conn = get_conn(conn_id);
  if (!conn)
    return 0;

  for (ushort i = 0; i < data_len; i++)
    conn->send_buf[conn->send_head++ % sizeof(conn->send_buf)] = ((ubyte *)data)[i];

  tcp_send_buffered_data();
  return 1;
}

int tcp_receive(uint conn_id, void *data, ushort data_len) {
  tcp_active_socket *conn = get_conn(conn_id);
  if (!conn)
    return 0;

  uint avail_len = conn->recv_head - conn->recv_tail;

  int len = data_len < avail_len ? data_len : avail_len;

  for (ushort i = 0; i < len; i++)
    ((ubyte *)data)[i] = conn->recv_buf[conn->recv_tail++ % sizeof(conn->recv_buf)];

  serial_printf("tcp_received called: %d", len);
  return len;
}

bool tcp_set_receiver(socket_id id, task *t) {
  return false;
}