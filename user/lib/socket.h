#pragma once
#include "cdefs.h"
#include "net_types.h"

EXTERN_C_BEGIN

typedef uint socket;
typedef uint server_socket;

server_socket socket_listen(ushort port);
socket socket_accept(server_socket socket);

socket socket_connect(ipv4_addr addr, ushort port);

int socket_recv(socket socket, void *buf, int len);
int socket_send(socket socket, void *buf, int len);

void socket_close(socket socket);

// UDP — remote addr/port are fixed at open time
socket socket_udp_open(ipv4_addr remote, ushort remote_port, ushort local_port);
int socket_udp_send(socket s, const void *buf, int len);
int socket_udp_recv(socket s, void *buf, int len);

EXTERN_C_END
