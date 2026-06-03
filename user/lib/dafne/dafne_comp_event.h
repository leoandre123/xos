#pragma once
#include "cdefs.h"
#include "dafne/dafne_event.h"
#include "types.h"

EXTERN_C_BEGIN

typedef enum {
  COMP_WINDOW_CREATE,
  COMP_WINDOW_PRESENT,
  COMP_WINDOW_DESTROY
} comp_event_type;

typedef struct {
  bool double_buffer;
  ushort width, height;
  char title[64];
  char icon_path[128];
} comp_create_window_event;

typedef struct {
  window_handle handle;
} comp_present_window_event;
typedef struct {
  window_handle handle;
} comp_destroy_window_event;

typedef struct {
  comp_event_type type;
  union {
    comp_create_window_event create_window;
    comp_present_window_event present_window;
    comp_destroy_window_event destroy_window;
  };
} comp_event;

EXTERN_C_END