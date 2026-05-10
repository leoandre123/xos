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

// UDP
socket socket_udp_open();
int socket_udp_send(socket s, ipv4_addr dst, ushort dst_port, void *buf,
                    int len);
int socket_udp_recv(socket s, void *buf, int len, ipv4_addr *src,
                    ushort *src_port);

EXTERN_C_END
