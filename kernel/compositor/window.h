#pragma once
#include "types.h"
#include "window_event.h"

#define WINDOW_MAX_COUNT   64
#define WINDOW_EVENT_QUEUE 16

typedef struct {
  bool exists;
  int owner_pid;
  ulong phys_base;
  ulong page_count;
  ulong client_vaddr; // framebuffer in owner's address space
  ulong comp_vaddr;   // framebuffer in compositor's address space
  ushort width, height;
  bool dirty;

  // Small event queue: compositor posts here, client drains via SYS_WINDOW_POLL
  window_event events[WINDOW_EVENT_QUEUE];
  int ev_head, ev_tail;
} kernel_window;
