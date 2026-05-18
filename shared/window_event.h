#pragma once

#include "fb_info.h"
#include "types.h"

typedef enum {

  // BASIC
  WET_CREATE,
  WET_CLOSE,
  WET_PAINT,

  WET_RESIZE,
  WET_MOVE,

  // INPUT
  WET_KEY_DOWN,
  WET_KEY_UP,
  WET_MOUSE,

} window_event_type;

typedef void *window_paint_handle;

typedef struct {
  int x;
  int y;
  int buttons;
  int scroll;
} window_mouse_event;

typedef struct {
  uint keycode;
  char character;
} window_key_event;

typedef struct {
  window_paint_handle paint_handle;
} window_paint_event;

typedef struct {
  int width;
  int height;
  int pitch;
} window_create_event;

typedef struct {
  window_event_type type;
  union {
    window_create_event create_event;
    window_mouse_event mouse_event;
    window_key_event key_event;
    window_paint_event paint_event;
    ubyte raw[16];
  };
} window_event;