#pragma once
#include "dafne/dafne_event.h"
#include "fb_info.h"
#include "image.h"

#define WINDOW_TITLE_BAR_HEIGHT 35
#define WINDOW_BUTTON_WIDTH     45
#define WINDOW_BORDER_RADIUS    7

#define WINDOW_MAX_COUNT 10

struct client;

typedef struct window {
  bool exists;
  window_handle handle;
  bool title_dirty;
  // bool focused;
  bool moved;
  bool hidden;
  bool hover_minimize;
  bool hover_close;
  bool presented;
  bool double_buffer;
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
  bitmap *icon;
  struct client *client;

  struct window *prev;
  struct window *next;
} window;
