#pragma once
#include "syscall.h"
#include "syscalls.h"
#include "types.h"

typedef int ipc_srv_handle;
typedef int channel_handle;

static inline ipc_srv_handle ipc_server(const char *identifier) {
  return syscall(SYS_IPC_SERVER, (ulong)identifier, 0, 0);
}

static inline channel_handle ipc_accept(ipc_srv_handle h) {
  return syscall(SYS_IPC_SERVER_ACCEPT, h, 0, 0);
}
static inline channel_handle ipc_accept_nb(ipc_srv_handle h) {
  return syscall(SYS_IPC_SERVER_ACCEPT_NB, h, 0, 0);
}

static inline channel_handle ipc_connect(const char *identifier) {
  return syscall(SYS_IPC_SERVER_CONNECT, (ulong)identifier, 0, 0);
}

static inline void channel_send(channel_handle h, const void *data, int len) {
  syscall(SYS_IPC_SEND, h, (ulong)data, len);
}
static inline int channel_recv(channel_handle h, void *buf, int len) {
  return syscall(SYS_IPC_RECV, h, (ulong)buf, len);
}
static inline int channel_recv_nb(channel_handle h, void *buf, int len) {
  return syscall(SYS_IPC_RECV_NB, h, (ulong)buf, len);
}
