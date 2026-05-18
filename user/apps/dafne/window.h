#pragma once
#include "fb_info.h"
#include "gfx.h"
#include "window_event.h"
#include "wm_event.h"

#define WINDOW_TITLE_BAR_HEIGHT 25
#define WINDOW_MAX_COUNT 10

typedef struct {
  bool exists;
  bool title_dirty;
  bool focused;
  bool moved;
  bool hidden;
  bool hover_minimize;
  bool hover_close;
  bool presented;
  fb_info front_buf;
  fb_info back_buf;
  void *usr_front_buf;
  void *usr_back_buf;
  // fb_info client;
  int x;
  int y;
  int prev_x;
  int prev_y;
  char title[32];
} window;

void window_focus(window *w);
void window_focus_next();
void window_move_to(window *w, int x, int y);
void window_show(window *w, bool show);

void window_post_event(window *w, window_event *event);
void window_swap_buffers(window *w);