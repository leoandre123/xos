#include "socket.h"
#include "syscall.h"
#include "syscalls.h"
#include "types.h"

/*
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
socket socket_udp_open(ipv4_addr remote, ushort remote_port,
                       ushort local_port) {
  return syscall(SYS_SOCKET_UDP, remote.value, remote_port, local_port);
}
int socket_udp_send(socket s, const void *buf, int len) {
  return syscall(SYS_SOCKET_SEND, s, (ulong)buf, len);
}
int socket_udp_recv(socket s, void *buf, int len) {
  return syscall(SYS_SOCKET_RECEIVE, s, (ulong)buf, len);
}
*/
socket_handle socket(socket_protocol protocol) {
  return syscall(SYS_SOCKET, protocol, 0, 0);
}
void socket_close(socket_handle h) { syscall(SYS_SOCKET_CLOSE, h, 0, 0); }
int socket_bind(socket_handle h, socket_addr *local) {
  return syscall(SYS_SOCKET_BIND, h, (ulong)local, 0);
}
int socket_bind_nic(socket_handle h, int nic) {
  return syscall(SYS_SOCKET_BIND_NIC, h, (ulong)nic, 0);
}
int socket_listen(socket_handle h) {
  return syscall(SYS_SOCKET_LISTEN, h, 0, 0);
}
socket_handle socket_accept(socket_handle h) {
  return syscall(SYS_SOCKET_ACCEPT, h, 0, 0);
}
int socket_connect(socket_handle h, socket_addr *remote) {
  return syscall(SYS_SOCKET_CONNECT, h, (ulong)remote, 0);
}

int socket_send(socket_handle h, void *buf, ushort len) {
  return syscall(SYS_SOCKET_SEND, h, (ulong)buf, len);
}

int socket_recv(socket_handle h, void *buf, ushort len) {
  return syscall(SYS_SOCKET_RECEIVE, h, (ulong)buf, len);
}
int socket_recv_nb(socket_handle h, void *buf, ushort len) {
  return syscall(SYS_SOCKET_RECEIVE_NB, h, (ulong)buf, len);
}