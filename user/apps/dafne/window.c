#include "window.h"
#include "compositor.h"
#include "types.h"

void window_focus(window *w) {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (&g_windows[i] == w) {
      if (!g_windows[i].focused) {
        g_windows[i].focused = true;
        g_windows[i].title_dirty = true;
      }
    } else {
      if (g_windows[i].focused) {
        g_windows[i].focused = false;
        g_windows[i].title_dirty = true;
      }
    }
  }
}

void window_focus_next() {
  bool focus_next = false;
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (!g_windows[i].exists)
      continue;
    if (focus_next) {
      g_windows[i].title_dirty = true;
      g_windows[i].focused = true;
      return;
    }
    if (g_windows[i].focused) {
      g_windows[i].focused = false;
      g_windows[i].title_dirty = true;
      focus_next = true;
    }
  }
  if (focus_next) {
    for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
      if (!g_windows[i].exists)
        continue;
      g_windows[i].focused = true;
      g_windows[i].title_dirty = true;
      return;
    }
  }
}

void window_move_to(window *w, int x, int y) {
  if (w->x == x && w->y == y)
    return;
  w->prev_x = w->x;
  w->prev_y = w->y;
  w->x = x;
  w->y = y;
  w->moved = true;
  w->title_dirty = true;
}

void window_show(window *w, bool show) {
  if (w->hidden == show) {
    w->hidden = !show;
    w->prev_x = w->x;
    w->prev_y = w->y;
    w->moved = true;
    w->title_dirty = true;
  }
}