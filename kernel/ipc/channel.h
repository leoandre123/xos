#pragma once
#include "ipc/handle.h"
#include "ipc/pipe.h"
#include "scheduler/process.h"
#include "scheduler/task.h"

// typedef int channel_handle;

typedef int ipc_srv_handle;

typedef struct {
  pid pids[2];
  pipe *p[2];
  task *listeners[2];
  bool is_closed[2];
} channel;

ipc_srv_handle ipc_server(const char *identifier);
channel_handle ipc_accept(ipc_srv_handle h);
channel_handle ipc_accept_nb(ipc_srv_handle h);
channel_handle ipc_connect(const char *identifier);
void channel_send(channel_handle h, const void *data, int len);
int channel_recv(channel_handle h, void *buf, int len);
bool channel_set_listener(channel_handle h, task *t);

void channel_close(channel *c, pid pid);