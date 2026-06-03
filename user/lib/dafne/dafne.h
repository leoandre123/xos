#pragma once
#include "cdefs.h"
#include "dafne/dafne_event.h"
#include "types.h"

EXTERN_C_BEGIN

typedef struct {
  ushort width;
  ushort height;
  const char *title;
} window_create_options;

// Create connection with dafne
bool dafne_connect();
void dafne_window_create(ushort width, ushort height, const char *title,
                         const char *icon_path);
void dafne_window_destroy(window_handle h);
bool dafne_wait_event(window_event *ev);
void dafne_window_present(window_handle h);

// window_handle window_open(ushort width, ushort height, const char *title);
//// Call after drawing to tell the compositor to repaint.
// void window_present(window_handle wh);
//// Blocking
// bool window_get_event(window_handle wh, window_event *ev);

EXTERN_C_END