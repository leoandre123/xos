#pragma once
#include "types.h"
#include "window_event.h"

typedef ulong window_handle;

typedef enum {
  CET_WINDOW_CREATE,
  CET_WINDOW_PRESENT,
} compositor_event_type;

typedef struct {
  window_handle handle;
  ulong comp_vaddr;
  ushort width, height;
  char title[64];
} compositor_create_window_event;

typedef struct {
  window_handle handle;
} compositor_present_window_event;

typedef struct {
  compositor_event_type type;
  union {
    compositor_create_window_event create_window;
    compositor_present_window_event present_window;
  };
} compositor_event;
