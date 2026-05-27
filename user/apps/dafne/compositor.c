#include "compositor.h"
#include "gfx.h"
#include "rect.h"
#include "syscall.h"
#include "window.h"
#include "window_event.h"
#include "wm_event.h"
#include <stdio.h>

fb_info g_backbuffer;
window g_windows[WINDOW_MAX_COUNT];

static int g_cursor_x = 0;
static int g_cursor_y = 0;
static int g_prev_cursor_x = 0;
static int g_prev_cursor_y = 0;

void compositor_set_cursor(int x, int y) {
  g_prev_cursor_x = g_cursor_x;
  g_prev_cursor_y = g_cursor_y;
  g_cursor_x = x;
  g_cursor_y = y;
}

#define CURSOR_W 12
#define CURSOR_H 20

static int sys_compositor_register(void) {
  return (int)syscall(SYS_WM_REGISTER, 0, 0, 0);
}

static int sys_compositor_poll(wm_event *ev) {
  return (int)syscall(SYS_WM_POLL, (ulong)ev, 0, 0);
}

static ulong rng_state = 88172645463325252ULL;

static inline ulong next_rand() {
  ulong x = rng_state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  rng_state = x;
  return x;
}

// Simple arrow cursor bitmap, 1 = white body, 2 = black outline, 0 =
// transparent
static const unsigned char g_cursor_bitmap[CURSOR_H][CURSOR_W] = {
    {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {2, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0}, {2, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0}, {2, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0},
    {2, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0, 0}, {2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 0},
    {2, 1, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0}, {2, 1, 1, 2, 1, 1, 2, 0, 0, 0, 0, 0},
    {2, 1, 2, 0, 2, 1, 1, 2, 0, 0, 0, 0}, {2, 2, 0, 0, 0, 2, 1, 1, 2, 0, 0, 0},
    {2, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

static void draw_cursor(fb_info *fb, int cx, int cy) {
  for (int row = 0; row < CURSOR_H; row++) {
    for (int col = 0; col < CURSOR_W; col++) {
      unsigned char v = g_cursor_bitmap[row][col];
      if (v == 0)
        continue;
      uint color = (v == 1) ? 0xffFFFFFF : 0xff000000;
      gfx_pixel(fb, (unsigned int)(cx + col), (unsigned int)(cy + row), color);
    }
  }
}

static rect cursor_rect(int x, int y) {
  return (rect){x, y, CURSOR_W, CURSOR_H};
}

static void render_title_bar(fb_info *dst, window *w) {
  uint bar = w->focused ? 0xff5294E2 : 0xff3A3A3A;
  uint min_bg = w->hover_minimize ? 0xff909090 : 0xff708090;
  uint cls_bg = w->hover_close ? 0xffFF4444 : 0xffCC0000;
  int tw = (int)w->front_buf.width;
  gfx_rect(dst, w->x, w->y, tw, WINDOW_TITLE_BAR_HEIGHT, bar);
  gfx_str(dst, w->x + 6, w->y + 5, w->title, 0xffFFFFFF);
  gfx_rect(dst, w->x + tw - 50, w->y + 2, 21, 21, min_bg);
  gfx_str(dst, w->x + tw - 47, w->y + 5, "_", 0xffFFFFFF);
  gfx_rect(dst, w->x + tw - 25, w->y + 2, 21, 21, cls_bg);
  gfx_str(dst, w->x + tw - 22, w->y + 5, "X", 0xffFFFFFF);
  w->title_dirty = 0;
}

void compositor_init() {
  g_backbuffer = gfx_create_surface(g_screen.width, g_screen.height);
  sys_compositor_register();
}

static void full_composite() {
  gfx_blit(&g_backbuffer, 0, 0, &g_desktop);
  fb_clear_dirty(&g_desktop);

  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &g_windows[i];
    if (!w->exists || w->hidden)
      continue;
    render_title_bar(&g_backbuffer, w);
    fb_clear_dirty(&w->front_buf);
    w->moved = false;
    gfx_blit(&g_backbuffer, w->x, w->y + WINDOW_TITLE_BAR_HEIGHT,
             &w->front_buf);
  }

  draw_cursor(&g_backbuffer, g_cursor_x, g_cursor_y);
  gfx_blit(&g_screen, 0, 0, &g_backbuffer);
}

__attribute__((noinline)) static bool
collect_dirty_regions(rect *dirty_region) {
  if (fb_is_dirty(&g_desktop))
    *dirty_region = g_desktop.dirty_region;

  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &g_windows[i];
    if (!w->exists || w->hidden)
      continue;
    if (w->title_dirty)
      *dirty_region =
          rect_union(*dirty_region, (rect){w->x, w->y, (int)w->front_buf.width,
                                           WINDOW_TITLE_BAR_HEIGHT});
    if (fb_is_dirty(&w->front_buf))
      *dirty_region = rect_union(*dirty_region,
                                 (rect){w->x + (int)w->front_buf.dirty_region.x,
                                        w->y + WINDOW_TITLE_BAR_HEIGHT +
                                            (int)w->front_buf.dirty_region.y,
                                        w->front_buf.dirty_region.w,
                                        w->front_buf.dirty_region.h});
  }

  // If cursor moved, include both old and new rects so the old one gets erased
  if (g_cursor_x != g_prev_cursor_x || g_cursor_y != g_prev_cursor_y) {
    *dirty_region = rect_union(*dirty_region,
                               cursor_rect(g_prev_cursor_x, g_prev_cursor_y));
    *dirty_region =
        rect_union(*dirty_region, cursor_rect(g_cursor_x, g_cursor_y));
  } else if (dirty_region->w > 0) {
    // Cursor didn't move but something else is dirty — redraw cursor at current
    // pos
    *dirty_region =
        rect_union(*dirty_region, cursor_rect(g_cursor_x, g_cursor_y));
  }

  return dirty_region->w;
}

__attribute__((noinline, optimize("O1"))) static void
composite_windows(rect *dirty_region) {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &g_windows[i];
    if (!w->exists || w->hidden)
      continue;
    rect title_rect = {w->x, w->y, (int)w->front_buf.width,
                       WINDOW_TITLE_BAR_HEIGHT};
    if (rect_intersect(title_rect, *dirty_region).w > 0)
      render_title_bar(&g_backbuffer, w);
    fb_clear_dirty(&w->front_buf);

    int client_y = w->y + WINDOW_TITLE_BAR_HEIGHT;

    // Window rect in screen space
    rect win_rect = {w->x, client_y, w->front_buf.width, w->front_buf.height};
    rect clip = rect_intersect(win_rect, *dirty_region);
    if (rect_equals(win_rect, *dirty_region) && clip.w == 0) {
      sys_write("CRIT CRIT CRIT\n");
    }
    if (clip.w == 0)
      continue;

    gfx_blit_region(&g_backbuffer, clip.x, clip.y, &w->front_buf, clip.x - w->x,
                    clip.y - client_y, clip.w, clip.h);
  }
}

__attribute__((optimize("O1"))) void compositor_run() {
  int any_moved = 0;
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (g_windows[i].exists && g_windows[i].moved) {
      any_moved = 1;
      break;
    }
  }
  if (any_moved) {
    full_composite();
    return;
  }

  rect dirty_region = {0};

  if (!collect_dirty_regions(&dirty_region))
    return;

  // --- 2. Desktop into damage region ---
  gfx_blit_region(&g_backbuffer, dirty_region.x, dirty_region.y, &g_desktop,
                  dirty_region.x, dirty_region.y, dirty_region.w,
                  dirty_region.h);
  fb_clear_dirty(&g_desktop);

  // --- 3. Windows clipped to damage region ---
  composite_windows(&dirty_region);
  // --- 4. Cursor on top ---
  draw_cursor(&g_backbuffer, g_cursor_x, g_cursor_y);

  // --- 5. Blit dirty region to screen ---

  gfx_blit_region(&g_screen, dirty_region.x, dirty_region.y, &g_backbuffer,
                  dirty_region.x, dirty_region.y, dirty_region.w,
                  dirty_region.h);
}

void compositor_handle_events() {
  wm_event ev;
  while (sys_compositor_poll(&ev)) {

    switch (ev.type) {
    case CET_WINDOW_PRESENT: {

      window_handle h = ev.present_window.handle;
      if (h < WINDOW_MAX_COUNT && g_windows[h].exists) {
        window *w = &g_windows[h];
        window_swap_buffers(w);
        fb_mark_dirty(&w->front_buf, 0, 0, w->front_buf.width,
                      w->front_buf.height);
        w->presented = true;
      }
      break;
    }
    case CET_WINDOW_CREATE: {
      // Find a free dafne window slot
      window *w = 0;
      for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
        if (!g_windows[i].exists) {
          w = &g_windows[i];
          break;
        }
      }
      if (!w)
        continue;

      // Wire up the shared framebuffer the kernel already mapped into our space
      w->front_buf.ptr = (uint *)ev.create_window.comp_fb[0];
      w->front_buf.width = ev.create_window.width;
      w->front_buf.height = ev.create_window.height;
      w->front_buf.pitch = ev.create_window.width * 4;
      w->front_buf.dirty_region.w = 0;

      w->back_buf.ptr = (uint *)ev.create_window.comp_fb[1];
      w->back_buf.width = ev.create_window.width;
      w->back_buf.height = ev.create_window.height;
      w->back_buf.pitch = ev.create_window.width * 4;
      w->back_buf.dirty_region.w = 0;

      w->usr_front_buf = ev.create_window.client_fb[0];
      w->usr_back_buf = ev.create_window.client_fb[1];

      w->x = 100;
      w->y = 100;
      w->exists = true;
      w->title_dirty = true;
      w->focused = false;
      w->moved = false;

      int i = 0;
      while (ev.create_window.title[i] && i < 31) {
        w->title[i] = ev.create_window.title[i];
        i++;
      }
      w->title[i] = '\0';

      window_focus(w);
      w->presented = true;

      window_event ev = {.type = WET_CREATE};
      ev.create_event.width = w->front_buf.width;
      ev.create_event.height = w->front_buf.height;
      ev.create_event.pitch = w->front_buf.pitch;
      window_post_event(w, &ev);
      break;
    }
    }
  }
}