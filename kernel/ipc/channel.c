#include "channel.h"
#include "io/logging.h"
#include "ipc/handle.h"
#include "ipc/pipe.h"
#include "ipc/pipe_server.h"
#include "memory/heap.h"
#include "scheduler/process.h"
#include "scheduler/scheduler.h"

#define MAX_SERVERS 32
#define MAX_PENDING 16

ulong hash64(const char *s) {
  ulong hash = 1469598103934665603ULL; // offset basis
  while (*s) {
    hash ^= (ubyte)(*s++);
    hash *= 1099511628211ULL; // FNV prime
  }
  return hash;
}

typedef struct {
  ulong identifier;
  task *listener;
  channel *incomming_connections[MAX_PENDING];
  int head, tail, count;
} server;

static server s_servers[MAX_SERVERS] = {0};
static pipe_server_handle create_server() {
  for (int i = 0; i < MAX_SERVERS; i++) {
    if (!s_servers[i].identifier) {
      return (pipe_server_handle)i;
    }
  }
  return -1;
}

static server *find_server(const char *identifier) {
  ulong hash = hash64(identifier);
  for (int i = 0; i < MAX_SERVERS; i++) {
    if (s_servers[i].identifier == hash) {
      return &s_servers[i];
    }
  }

  return 0;
}

static server *get_server(pipe_server_handle h) {
  if (h < 0 || h >= MAX_SERVERS)
    return 0;
  return &s_servers[h];
}

static void free_server(server *s) {
  s->identifier = 0;
}

static channel *dequeue_incomming(server *s) {
  if (s->count == 0)
    return 0;
  channel *ch = s->incomming_connections[s->head];
  s->head = (s->head + 1) % MAX_PENDING;
  s->count--;
  return ch;
}

static void enqueue_incomming(server *s, channel *ch) {
  if (s->count == MAX_PENDING)
    return;
  s->incomming_connections[s->tail] = ch;
  s->tail = (s->tail + 1) % MAX_PENDING;
  s->count++;
}

ipc_srv_handle ipc_server(const char *identifier) {
  if (find_server(identifier))
    return -1;
  pipe_server_handle hnd = create_server();
  if (hnd == -1)
    return -1;
  server *srv = get_server(hnd);
  srv->identifier = hash64(identifier);
  srv->listener = scheduler_current();
  return hnd;
}
channel_handle ipc_accept(ipc_srv_handle h) {
  task *t = scheduler_current();
  process *p = t->owner;
  server *s = get_server(h);
  if (!s)
    return -1;

  channel *ch = dequeue_incomming(s);
  if (!ch) {
    if (s->listener)
      return -1;
    s->listener = t;

    task_set_blocked(t);
    schedule();
    ch = dequeue_incomming(s);
    if (!ch)
      return -1;
  }

  ch->pids[1] = p->pid;
  return handle_alloc(p, HANDLE_CHANNEL, ch);
}

channel_handle ipc_accept_nb(ipc_srv_handle h) {

  server *s = get_server(h);
  if (!s)
    return -1;

  channel *ch = dequeue_incomming(s);
  if (!ch)
    return -1;

  task *t = scheduler_current();
  process *p = t->owner;

  ch->pids[1] = p->pid;
  return handle_alloc(p, HANDLE_CHANNEL, ch);
}

channel_handle ipc_connect(const char *identifier) {

  server *s = find_server(identifier);
  if (!s)
    return -1;
  process *p = scheduler_current()->owner;

  channel *ch = kmalloc(sizeof(channel));
  ch->pids[0] = p->pid;
  ch->pids[1] = 0;
  ch->p[0] = pipe_create();
  ch->p[1] = pipe_create();
  ch->listeners[0] = 0;
  ch->listeners[1] = 0;

  enqueue_incomming(s, ch);
  if (s->listener) {
    task_set_ready(s->listener);
    s->listener = 0;
  }
  return handle_alloc(p, HANDLE_CHANNEL, ch);
}

void channel_send(channel_handle h, const void *data, int len) {
  process *p = scheduler_current()->owner;
  handle_entry *handle = handle_get(p, h);
  if (!handle)
    return;
  channel *ch = handle->ptr;
  if (!ch)
    return;

  if (ch->pids[0] == p->pid) {
    pipe_write(ch->p[0], data, len);
    if (ch->listeners[1]) {
      task_set_ready(ch->listeners[1]);
      ch->listeners[1] = 0;
    }
  } else if (ch->pids[1] == p->pid) {
    pipe_write(ch->p[1], data, len);
    if (ch->listeners[0]) {
      task_set_ready(ch->listeners[0]);
      ch->listeners[0] = 0;
    }
  } else {
    klogf(LOG_WARNING, "Unauthorized usage");
  }
}
int channel_recv(channel_handle h, void *buf, int len) {
  process *p = scheduler_current()->owner;
  handle_entry *handle = handle_get(p, h);
  if (!handle)
    return 0;
  channel *ch = handle->ptr;
  if (!ch)
    return 0;

  if (ch->pids[0] == p->pid) {
    return pipe_read(ch->p[1], buf, len);
  } else if (ch->pids[1] == p->pid) {
    return pipe_read(ch->p[0], buf, len);
  } else {
    klogf(LOG_WARNING, "Unauthorized usage");
    return 0;
  }
}
bool channel_set_listener(channel_handle h, task *t) {
  handle_entry *handle = handle_get(t->owner, h);
  if (!handle)
    return 0;
  channel *ch = handle->ptr;
  if (!ch)
    return 0;

  if (t->owner->pid == ch->pids[0]) {
    if (ch->listeners[0])
      return 0;
    ch->listeners[0] = t;
    return true;
  }
  if (t->owner->pid == ch->pids[1]) {
    if (ch->listeners[1])
      return 0;
    ch->listeners[1] = t;
    return true;
  }
  return false;
}