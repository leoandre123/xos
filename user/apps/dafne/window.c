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

void window_post_event(window *w, window_event *event) {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (&g_windows[i] == w) {
      syscall(SYS_WINDOW_POST_EVENT, i, (ulong)event, 0);
    }
  }
}

void window_swap_buffers(window *w) {
  void *tmp = w->front_buf.ptr;
  w->front_buf.ptr = w->back_buf.ptr;
  w->back_buf.ptr = tmp;

  tmp = w->usr_front_buf;
  w->usr_front_buf = w->usr_back_buf;
  w->usr_back_buf = tmp;
}