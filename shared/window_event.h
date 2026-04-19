#pragma once

#include "types.h"

typedef enum {

  WET_KEY_DOWN,
  WET_KEY_UP,

  WET_MOUSE,

  WET_RESIZE,
  WET_MOVE,

  WET_CLOSE,

} window_event_type;

typedef struct {
  int x;
  int y;
  int buttons;
} window_mouse_event;

typedef struct {
  uint keycode;
  char character;
} window_key_event;

typedef struct {
  window_event_type type;
  union {
    window_mouse_event mouse_event;
    window_key_event key_event;
    ubyte raw[16];
  };
} window_event;