#pragma once
#include "cdefs.h"
#include "net_types.h"

EXTERN_C_BEGIN

typedef uint socket_handle;

socket_handle socket(socket_protocol protocol);       // Create socket
void socket_close(socket_handle handle);              // Closes socket
int socket_bind(socket_handle h, socket_addr *local); // Bind to local address
int socket_bind_nic(socket_handle h, int nic_id);
int socket_listen(socket_handle h); // Listens to local interface (tcp only)
socket_handle
socket_accept(socket_handle h); // Return socket to incoming connection
int socket_connect(socket_handle h,
                   socket_addr *remote); // Connects to remote endpoint
int socket_recv(socket_handle handle, void *buf, ushort len);
int socket_send(socket_handle handle, void *buf, ushort len);
int socket_recv_nb(socket_handle h, void *buf, ushort len);
EXTERN_C_END
