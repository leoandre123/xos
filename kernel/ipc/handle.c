#include "handle.h"
#include "ipc/channel.h"
#include "ipc/pipe.h"
#include "panic.h"
#include "scheduler/process.h"

base_handle handle_alloc(process *p, handle_type type, void *ptr) {
  for (int i = 0; i < MAX_HANDLES; i++) {
    if (p->handles[i].type == HANDLE_NONE) {
      p->handles[i].type = type;
      p->handles[i].ptr = ptr;
      return i;
    }
  }
  return -1;
}

handle_entry *handle_get(process *p, base_handle h) {
  if (h < 0 || h >= MAX_HANDLES)
    return 0;
  if (p->handles[h].type == HANDLE_NONE)
    return 0;
  return &p->handles[h];
}

void handle_close_all(process *p) {
  for (int i = 0; i < MAX_HANDLES; i++) {
    handle_entry *h = &p->handles[i];
    switch (h->type) {
    case HANDLE_NONE:
      continue;
    case HANDLE_CHANNEL: channel_close(h->ptr, p->pid); break;
    case HANDLE_PIPE_READ: pipe_close_read(h->ptr); break;
    case HANDLE_PIPE_WRITE: pipe_close_write(h->ptr); break;
    case HANDLE_FILE: break;
    default:
      panic("Not implemented");
    }
  }
}