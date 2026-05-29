#include "compositor.h"
#include "dafne/dafne_comp_event.h"
#include "dafne/dafne_event.h"
#include "fb_info.h"
#include "font.h"
#include "gfx.h"
#include "handles.h"
#include "ipc/channel.h"
#include "memory.h"
#include "rect.h"
#include "syscall.h"
#include "time.h"
#include "window.h"
#include "window/ui/retained_ui_internal.h"

#define MAX_CLIENTS 16

typedef struct client {
  bool exists;
  channel_handle ch;
  window *win;
} client;

static window s_window_pool[WINDOW_MAX_COUNT] = {0};
static client s_client_pool[MAX_CLIENTS] = {0};

ipc_srv_handle g_srv_handle = {0};
channel_handle g_channel_handles[WINDOW_MAX_COUNT] = {0};

fb_info g_screen = {0};     // Entire screen
fb_info g_backbuffer = {0}; // Entire screen backbuffer
fb_info g_desktop = {0};    //

static mouse_event g_mouse = {0};

static ulong g_time = 0;

static int g_drag_off_x = {0};
static int g_drag_off_y = {0};

static window *s_windows = 0;
static window *s_last_window = 0;
static window *s_dragged_window = 0;

static bool status_bar_dirty;

static client *alloc_client() {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (!s_client_pool[i].exists)
      return &s_client_pool[i];
  }
  return 0;
}

static window_handle alloc_window_handle() {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    if (!s_window_pool[i].exists)
      return i;
  }
  return -1;
}
static window *get_window(window_handle h) {
  if (h < 0 || h >= WINDOW_MAX_COUNT)
    return 0;

  return &s_window_pool[h];
}

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
      uint color = (v == 1) ? RGB(255, 255, 255) : RGB(0, 0, 0);
      gfx_pixel(fb, (uint)(cx + col), (uint)(cy + row), color);
    }
  }
}

static rect cursor_rect(int x, int y) {
  return (rect){x, y, CURSOR_W, CURSOR_H};
}

static void render_title_bar(fb_info *dst, window *w) {
  uint bar = w == s_windows ? 0xff5294E2 : 0xff3A3A3A;
  uint min_bg = w->hover_minimize ? 0xff909090 : 0xff708090;
  uint cls_bg = w->hover_close ? 0xffFF4444 : 0xffCC0000;
  int tw = (int)w->front_buf.width;

  int btn_marg_x = (WINDOW_BUTTON_WIDTH - FONT_GLYPH_WIDTH) / 2;
  int btn_marg_y = (WINDOW_TITLE_BAR_HEIGHT - FONT_GLYPH_WIDTH) / 2;

  gfx_rect_rounded(dst, w->x, w->y, tw, WINDOW_TITLE_BAR_HEIGHT, bar,
                   WINDOW_BORDER_RADIUS, WINDOW_BORDER_RADIUS, 0, 0);
  gfx_str(dst, w->x + 6, w->y + btn_marg_y, w->title, 0xffFFFFFF);

  gfx_rect_rounded(dst, w->x + tw - WINDOW_BUTTON_WIDTH, w->y,
                   WINDOW_BUTTON_WIDTH, WINDOW_TITLE_BAR_HEIGHT, cls_bg, 0,
                   WINDOW_BORDER_RADIUS, 0, 0);
  gfx_str(dst, w->x + tw - WINDOW_BUTTON_WIDTH + btn_marg_x, w->y + btn_marg_y,
          "X", 0xffFFFFFF);

  gfx_rect(dst, w->x + tw - WINDOW_BUTTON_WIDTH - WINDOW_BUTTON_WIDTH, w->y,
           WINDOW_BUTTON_WIDTH, WINDOW_TITLE_BAR_HEIGHT, min_bg);
  gfx_str(dst,
          w->x + tw - WINDOW_BUTTON_WIDTH - WINDOW_BUTTON_WIDTH + btn_marg_x,
          w->y + btn_marg_y, "_", 0xffFFFFFF);

  w->title_dirty = 0;
}

static void client_post_event(client *client, window_event *event) {
  channel_send(client->ch, event, sizeof(window_event));
}

static void send_paint_events() {

  for (int i = 0; i < MAX_CLIENTS; i++) {
    client *c = &s_client_pool[i];
    if (!c->exists)
      continue;
    if (!c->win)
      continue;
    if (!c->win->presented)
      continue;
    c->win->presented = false;
    window_event ev = {.type = WET_PAINT};
    ev.paint_event.paint_handle =
        (c->win->double_buffer ? c->win->usr_back_buf : c->win->usr_back_buf);
    client_post_event(c, &ev);
    // sys_write("send event");
  }
}

static window *get_window_at_pos(int x, int y) {
  for (window *win = s_windows; win; win = win->next) {
    if (win->hidden)
      continue;
    if (x >= win->x && x < win->x + (int)win->front_buf.width && y >= win->y &&
        y < win->y + WINDOW_TITLE_BAR_HEIGHT + (int)win->front_buf.height)
      return win;
  }
  return 0;
}

static bool hit_minimize(window *w, int x, int y) {
  int bx = w->x + (int)w->front_buf.width - WINDOW_BUTTON_WIDTH -
           WINDOW_BUTTON_WIDTH;
  return PT_IN_RECT(x, y, bx, w->y, WINDOW_BUTTON_WIDTH,
                    WINDOW_TITLE_BAR_HEIGHT);
}

static bool hit_close(window *w, int x, int y) {
  int bx = w->x + (int)w->front_buf.width - WINDOW_BUTTON_WIDTH;
  return PT_IN_RECT(x, y, bx, w->y, WINDOW_BUTTON_WIDTH,
                    WINDOW_TITLE_BAR_HEIGHT);
}

static bool hit_title_bar(window *w, int x, int y) {
  return x >= w->x && x < w->x + (int)w->front_buf.width && y >= w->y &&
         y < w->y + WINDOW_TITLE_BAR_HEIGHT;
}

// Add window to linked list
static void add_window(window *win) {
  if (s_windows) {
    s_windows->prev = win;
    win->next = s_windows;
    s_windows->moved = true;
  } else {
    win->next = 0;
    s_last_window = win;
  }

  win->prev = 0;
  s_windows = win;
}
static void remove_window(window *win) {
  if (win->prev)
    win->prev->next = win->next;
  if (win->next)
    win->next->prev = win->prev;

  if (win == s_last_window)
    s_last_window = win->prev;

  if (win == s_windows)
    s_last_window = win->next;
}

void compositor_update_desktop() {
  ulong ts = sys_unix_time();
  if (status_bar_dirty || ts != g_time) {
    gfx_rect(&g_desktop, 0, 0, g_desktop.width, STATUS_BAR_HEIGHT,
             RGB(109, 94, 214));

    int x = 0;
    char name[4] = "\0\0\0\0";
    for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
      window *win = &s_window_pool[i];
      if (!win->exists)
        continue;
      memcpy(name, win->title, 3);
      gfx_rect(&g_desktop, x, 5, FONT_GLYPH_WIDTH * 3, FONT_GLYPH_HEIGHT,
               RGB(255, 0, 0));
      gfx_str(&g_desktop, x, 5, name, RGB(0, 255, 0));

      x += FONT_GLYPH_WIDTH * 4;
    }

    g_time = ts;
    datetime dt = to_datetime(ts);

    gfx_strf(&g_desktop, g_desktop.width - FONT_GLYPH_WIDTH * 10, 5,
             RGB(255, 94, 214), "%02u:%02u:%02u\n", dt.hour, dt.min, dt.sec);
    gfx_strf(&g_desktop, g_desktop.width - FONT_GLYPH_WIDTH * 12, 30,
             RGB(255, 94, 214), "%04u-%02u-%02u", dt.year, dt.month, dt.day);
  }
}

void compositor_init() {
  g_backbuffer = gfx_create_surface(g_screen.width, g_screen.height);

  g_srv_handle = ipc_server("dafne");
  if (g_srv_handle != -1) {
    sys_write("[DAFNE] Server created\n");
  } else {
    sys_write("[DAFNE] Server failed\n");
  }
}

static void full_composite() {
  gfx_blit(&g_backbuffer, 0, 0, &g_desktop);
  fb_clear_dirty(&g_desktop);

  for (window *w = s_last_window; w; w = w->prev) {
    if (!w->exists || w->hidden)
      continue;
    render_title_bar(&g_backbuffer, w);
    fb_clear_dirty(&w->front_buf);
    w->moved = false;
    gfx_blit_rounded(&g_backbuffer, w->x, w->y + WINDOW_TITLE_BAR_HEIGHT,
                     &w->front_buf, 0, 0, 7, 7);
  }

  draw_cursor(&g_backbuffer, g_cursor_x, g_cursor_y);
  gfx_blit(&g_screen, 0, 0, &g_backbuffer);
}

__attribute__((noinline)) static bool
collect_dirty_regions(rect *dirty_region) {
  if (fb_is_dirty(&g_desktop))
    *dirty_region = g_desktop.dirty_region;

  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &s_window_pool[i];
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
  for (window *w = s_last_window; w; w = w->prev) {
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

    gfx_blit_region_rounded4(&g_backbuffer, clip.x, clip.y, &w->front_buf,
                             clip.x - w->x, clip.y - client_y, clip.w, clip.h,
                             0, 0, 7, 7);
  }
}

__attribute__((optimize("O1"))) void compositor_run() {
  bool any_moved = false;
  for (window *w = s_last_window; w; w = w->prev) {
    if (w->moved) {
      any_moved = true;
      break;
    }
  }
  if (any_moved) {
    full_composite();
    send_paint_events();
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
  send_paint_events();
}

void handle_incoming() {
  channel_handle ch;
  while ((ch = ipc_accept_nb(g_srv_handle)) != INVALID_HANDLE) {
    sys_write("[DAFNE] Incoming connection...\n");

    client *client = alloc_client();
    client->exists = true;
    client->ch = ch;
    client->win = 0;
  }
}

void handle_events() {
  // MAYBE LINKED LIST INSTEAD

  comp_event ev;

  for (int i = 0; i < MAX_CLIENTS; i++) {
    client *client = &s_client_pool[i];
    if (!client->exists)
      continue;
    while (channel_recv_nb(client->ch, &ev, sizeof(comp_event)) ==
           sizeof(comp_event)) {
      switch (ev.type) {
      case COMP_WINDOW_PRESENT: {
        window_handle h = ev.present_window.handle;
        window *w = get_window(h);
        if (!w)
          continue;

        if (w->double_buffer)
          window_swap_buffers(w);
        fb_mark_dirty(&w->front_buf, 0, 0, w->front_buf.width,
                      w->front_buf.height);
        w->presented = true;
        break;
      }
      case COMP_WINDOW_CREATE: {
        sys_write("[DAFENE] create window\n");
        window_handle h = alloc_window_handle();
        if (h == INVALID_HANDLE)
          continue;
        window *w = get_window(h);
        if (!w)
          continue;

        ulong fb_size =
            (ulong)ev.create_window.width * (ulong)ev.create_window.height * 4;

        void *client_fb;
        void *comp_fb =
            malloc_shared((ev.create_window.double_buffer ? 2 : 1) * fb_size,
                          client->ch, &client_fb);

        w->double_buffer = ev.create_window.double_buffer;

        w->front_buf.ptr = (uint *)comp_fb;
        w->front_buf.width = ev.create_window.width;
        w->front_buf.height = ev.create_window.height;
        w->front_buf.pitch = ev.create_window.width * 4;
        w->front_buf.dirty_region.w = 0;
        w->usr_front_buf = client_fb;

        if (ev.create_window.double_buffer) {
          w->back_buf.ptr = (uint *)(comp_fb + fb_size);
          w->back_buf.width = ev.create_window.width;
          w->back_buf.height = ev.create_window.height;
          w->back_buf.pitch = ev.create_window.width * 4;
          w->back_buf.dirty_region.w = 0;
          w->usr_back_buf = (client_fb + fb_size);
        }

        w->x = 100;
        w->y = 100;
        w->exists = true;
        w->title_dirty = true;
        // w->focused = false;
        w->moved = true;
        w->client = (struct client *)client;
        client->win = w;

        int i = 0;
        while (ev.create_window.title[i] && i < 31) {
          w->title[i] = ev.create_window.title[i];
          i++;
        }
        w->title[i] = '\0';
        w->presented = true;

        add_window(w);

        window_event ev = {.type = WET_CREATE};
        ev.create_event.width = w->front_buf.width;
        ev.create_event.height = w->front_buf.height;
        ev.create_event.pitch = w->front_buf.pitch;
        ev.create_event.handle = h;
        client_post_event(client, &ev);
        break;
      }
      }
    }
  }
}

void compositor_handle_events() {
  handle_incoming();
  handle_events();
}

void window_focus(window *w) {
  if (w == s_windows)
    return;

  if (w == s_last_window) {
    s_last_window = w->prev;
  }

  // s_windows->focused = false;
  s_windows->moved = true;
  if (w->prev)
    w->prev->next = w->next;
  if (w->next)
    w->next->prev = w->prev;

  w->prev = 0;
  w->next = s_windows;
  s_windows->prev = w;
  s_windows = w;

  // w->focused = true;
  w->moved = true;
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

void window_swap_buffers(window *w) {
  void *tmp = w->front_buf.ptr;
  w->front_buf.ptr = w->back_buf.ptr;
  w->back_buf.ptr = tmp;

  tmp = w->usr_front_buf;
  w->usr_front_buf = w->usr_back_buf;
  w->usr_back_buf = tmp;
}

void compositor_handle_input() {
  mouse_event ev;
  if (sys_read_mouse(&ev)) {
    compositor_set_cursor(ev.x, ev.y);
    bool left_down = (ev.buttons & 1) && !(g_mouse.buttons & 1);
    bool left_up = !(ev.buttons & 1) && (g_mouse.buttons & 1);
    g_mouse = ev;

    if (left_up)
      s_dragged_window = 0;

    // Continue drag
    if (s_dragged_window && (ev.buttons & 1)) {
      s_dragged_window->prev_x = s_dragged_window->x;
      s_dragged_window->prev_y = s_dragged_window->y;
      s_dragged_window->x = ev.x - g_drag_off_x;
      s_dragged_window->y = ev.y - g_drag_off_y;
      s_dragged_window->moved = true;
      return;
    }

    // Update hover state for all windows
    for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
      window *w = &s_window_pool[i];
      if (!w->exists || w->hidden)
        continue;
      bool hm = hit_minimize(w, ev.x, ev.y);
      bool hc = hit_close(w, ev.x, ev.y);
      if (hm != w->hover_minimize || hc != w->hover_close) {
        w->hover_minimize = hm;
        w->hover_close = hc;
        w->title_dirty = true;
      }
    }

    window *w = get_window_at_pos(ev.x, ev.y);
    if (w) {
      if (left_down) {
        window_focus(w);
        if (hit_minimize(w, ev.x, ev.y)) {
          window_show(w, false);
        } else if (hit_close(w, ev.x, ev.y)) {
          window_event ev = {.type = WET_CLOSE};
          client_post_event(w->client, &ev);
        } else if (hit_title_bar(w, ev.x, ev.y)) {
          s_dragged_window = w;
          g_drag_off_x = ev.x - w->x;
          g_drag_off_y = ev.y - w->y;
        }
      }

      // Only forward to client if not dragging and click is in the client area
      if (!s_dragged_window && !hit_title_bar(w, ev.x, ev.y)) {
        window_event wev = {.type = WET_MOUSE};
        wev.mouse_event.x = ev.x - w->x;
        wev.mouse_event.y = ev.y - w->y - WINDOW_TITLE_BAR_HEIGHT;
        wev.mouse_event.buttons = ev.buttons;
        wev.mouse_event.scroll = ev.scroll;
        client_post_event(w->client, &wev);
      }
    }
  }

  // Forward input to focused client window
  if (s_windows) {
    KeyEvent key = sys_read_key_nb();
    if (key.character || key.code) {
      window_event wev = {.type = WET_KEY_DOWN};
      wev.key_event.keycode = key.code;
      wev.key_event.character = key.character;
      client_post_event(s_windows->client, &wev);
    }
  }
}