#include "socket.h"
#include "syscall.h"
#include "syscalls.h"

server_socket socket_listen(ushort port) {
  return syscall(SYS_SOCKET_LISTEN, port, 0, 0);
}
socket socket_accept(server_socket socket) {
  return syscall(SYS_SOCKET_ACCEPT, socket, 0, 0);
}

socket socket_connect(ipv4_addr addr, ushort port) {
  return syscall(SYS_SOCKET_CONNECT, (addr.value), port, 0);
}

int socket_recv(socket socket, void *buf, int len) {
  return syscall(SYS_SOCKET_RECEIVE, socket, (ulong)buf, len);
}
int socket_send(socket socket, void *buf, int len) {
  return syscall(SYS_SOCKET_SEND, socket, (ulong)buf, len);
}

void socket_close(socket socket) { syscall(SYS_SOCKET_CLOSE, socket, 0, 0); }

// UDP
socket socket_udp_open() {}
int socket_udp_send(socket s, ipv4_addr dst, ushort dst_port, void *buf,
                    int len) {}
int socket_udp_recv(socket s, void *buf, int len, ipv4_addr *src,
                    ushort *src_port) {}
