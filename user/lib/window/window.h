#pragma once
#include "cdefs.h"
#include "gfx.h"
#include "window/ui/immediate_ui.h"
#include "window_event.h"

EXTERN_C_BEGIN

typedef ulong window_handle;

typedef struct {
  ushort width;
  ushort height;
  const char *title;
} window_create_options;

// typedef struct {
//   window_handle handle;
//   fb_info fb;
// } window;

window_handle window_open(ushort width, ushort height, const char *title);

// Call after drawing to tell the compositor to repaint.
void window_present(window_handle wh);

// Non-blocking. Returns 1 and fills *ev if an event is pending, 0 otherwise.
int window_poll_event(window_handle wh, window_event *ev);

void window_begin_paint(window_handle wh, ui_ctx *ctx);
void window_end_paint(window_handle wh);
void window_get_framebuffer(window_handle wh, fb_info *fb);

EXTERN_C_END