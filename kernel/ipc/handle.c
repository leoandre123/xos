#include "handle.h"
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