#pragma once
#include "types.h"
#include "window_event.h"

typedef ulong window_handle;

typedef enum {
  CET_WINDOW_CREATE,
  CET_WINDOW_PRESENT,
} wm_event_type;

typedef struct {
  window_handle handle;
  void *client_fb[2];
  void *comp_fb[2];
  ushort width, height;
  char title[64];
} wm_create_window_event;

typedef struct {
  window_handle handle;
} wm_present_window_event;

typedef struct {
  wm_event_type type;
  union {
    wm_create_window_event create_window;
    wm_present_window_event present_window;
  };
} wm_event;
