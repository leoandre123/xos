#include "compositor.h"
#include "compositor_event.h"
#include "gfx.h"
#include "rect.h"
#include "window.h"

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
  return (int)syscall(SYS_COMPOSITOR_REGISTER, 0, 0, 0);
}

static int sys_compositor_poll(compositor_event *ev) {
  return (int)syscall(SYS_COMPOSITOR_POLL, (ulong)ev, 0, 0);
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
      uint color = (v == 1) ? 0x00FFFFFF : 0x00000000;
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
  int tw = (int)w->surface.width;
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
    fb_clear_dirty(&w->surface);
    w->moved = false;
    gfx_blit(&g_backbuffer, w->x, w->y + WINDOW_TITLE_BAR_HEIGHT, &w->surface);
  }

  draw_cursor(&g_backbuffer, g_cursor_x, g_cursor_y);
  gfx_blit(&g_screen, 0, 0, &g_backbuffer);
}

void compositor_run() {
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

  // --- 1. Collect damage: desktop, windows, and cursor (old + new pos) ---
  rect dirty_region = {0};

  if (fb_is_dirty(&g_desktop))
    dirty_region = g_desktop.dirty_region;

  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &g_windows[i];
    if (!w->exists || w->hidden)
      continue;
    if (w->title_dirty)
      dirty_region =
          rect_union(dirty_region, (rect){w->x, w->y, (int)w->surface.width,
                                          WINDOW_TITLE_BAR_HEIGHT});
    if (fb_is_dirty(&w->surface))
      dirty_region =
          rect_union(dirty_region, (rect){w->x + (int)w->surface.dirty_region.x,
                                          w->y + WINDOW_TITLE_BAR_HEIGHT +
                                              (int)w->surface.dirty_region.y,
                                          w->surface.dirty_region.w,
                                          w->surface.dirty_region.h});
  }

  // If cursor moved, include both old and new rects so the old one gets erased
  if (g_cursor_x != g_prev_cursor_x || g_cursor_y != g_prev_cursor_y) {
    dirty_region =
        rect_union(dirty_region, cursor_rect(g_prev_cursor_x, g_prev_cursor_y));
    dirty_region =
        rect_union(dirty_region, cursor_rect(g_cursor_x, g_cursor_y));
  } else if (dirty_region.w > 0) {
    // Cursor didn't move but something else is dirty — redraw cursor at current
    // pos
    dirty_region =
        rect_union(dirty_region, cursor_rect(g_cursor_x, g_cursor_y));
  }

  if (dirty_region.w == 0)
    return;

  // --- 2. Desktop into damage region ---
  gfx_blit_region(&g_backbuffer, dirty_region.x, dirty_region.y, &g_desktop,
                  dirty_region.x, dirty_region.y, dirty_region.w,
                  dirty_region.h);
  fb_clear_dirty(&g_desktop);

  // --- 3. Windows clipped to damage region ---
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &g_windows[i];
    if (!w->exists || w->hidden)
      continue;
    rect title_rect = {w->x, w->y, (int)w->surface.width,
                       WINDOW_TITLE_BAR_HEIGHT};
    if (rect_intersect(title_rect, dirty_region).w > 0)
      render_title_bar(&g_backbuffer, w);
    fb_clear_dirty(&w->surface);

    int client_y = w->y + WINDOW_TITLE_BAR_HEIGHT;
    rect win_rect = {w->x, client_y, w->surface.width, w->surface.height};
    rect clip = rect_intersect(win_rect, dirty_region);
    if (clip.w == 0)
      continue;
    gfx_blit_region_rounded(&g_backbuffer, clip.x, clip.y, &w->surface,
                            clip.x - w->x, clip.y - client_y, clip.w, clip.h,
                            WINDOW_BORDER_RADIUS);
  }

  // --- 4. Cursor on top ---
  draw_cursor(&g_backbuffer, g_cursor_x, g_cursor_y);

  // --- 5. Blit dirty region to screen ---
  gfx_blit_region(&g_screen, dirty_region.x, dirty_region.y, &g_backbuffer,
                  dirty_region.x, dirty_region.y, dirty_region.w,
                  dirty_region.h);
}

void compositor_handle_events() {
  compositor_event ev;
  while (sys_compositor_poll(&ev)) {
    if (ev.type == CET_WINDOW_PRESENT) {
      window_handle h = ev.present_window.handle;
      if (h < WINDOW_MAX_COUNT && g_windows[h].exists) {
        window *w = &g_windows[h];
        fb_mark_dirty(&w->surface, 0, 0, w->surface.width, w->surface.height);
      }
      continue;
    }
    if (ev.type != CET_WINDOW_CREATE)
      continue;

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
    w->surface.ptr = (uint *)ev.create_window.comp_vaddr;
    w->surface.width = ev.create_window.width;
    w->surface.height = ev.create_window.height;
    w->surface.pitch = ev.create_window.width * 4;
    w->surface.dirty_region.w = 0;

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
  }
}