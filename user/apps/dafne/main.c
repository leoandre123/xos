#include "compositor.h"
#include "font.h"
#include "gfx.h"
#include "keyboard.h"
#include "keys.h"
#include "memory.h"
#include "mouse.h"
#include "syscall.h"
#include "time.h"
#include "window.h"
#include "window_event.h"
#include "wm_event.h"

#define MAX_ANSI_ARGUMENTS 16

#define IS_ALPHA(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z')

#define BG 0x00C4D6B0
#define TB 0x00381D2A
#define TERM_BG 0x003E363F
#define TERM_FG 0x00DD403A

fb_info g_screen;
fb_info g_desktop;
static mouse_event g_mouse;

static ulong g_time;

static window_handle focused_handle(void) {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++)
    if (g_windows[i].exists && g_windows[i].focused)
      return (window_handle)i;
  return (window_handle)-1;
}
static window *focused_window(void) {
  for (int i = 0; i < WINDOW_MAX_COUNT; i++)
    if (g_windows[i].exists && g_windows[i].focused)
      return &g_windows[i];
  return 0;
}

static window_handle get_window_at_pos(int x, int y) {
  for (int i = WINDOW_MAX_COUNT - 1; i >= 0; i--) {
    if (!g_windows[i].exists || g_windows[i].hidden)
      continue;
    if (x >= g_windows[i].x &&
        x < g_windows[i].x + (int)g_windows[i].front_buf.width &&
        y >= g_windows[i].y &&
        y < g_windows[i].y + WINDOW_TITLE_BAR_HEIGHT +
                (int)g_windows[i].front_buf.height)
      return (window_handle)i;
  }
  return (window_handle)-1;
}

static bool hit_minimize(window *w, int x, int y) {
  int bx = w->x + (int)w->front_buf.width - 50;
  return x >= bx && x < bx + 21 && y >= w->y + 2 && y < w->y + 23;
}

static bool hit_close(window *w, int x, int y) {
  int bx = w->x + (int)w->front_buf.width - 25;
  return x >= bx && x < bx + 21 && y >= w->y + 2 && y < w->y + 23;
}

static bool hit_title_bar(window *w, int x, int y) {
  return x >= w->x && x < w->x + (int)w->front_buf.width && y >= w->y &&
         y < w->y + WINDOW_TITLE_BAR_HEIGHT;
}

static void update_desktop() {
  char time[20];
  ulong ts = sys_time();
  if (ts != g_time) {
    g_time = ts;
    datetime dt = to_datetime(ts);
    gfx_strf(&g_desktop, g_desktop.width - FONT_GLYPH_WIDTH * 10,
             g_desktop.height - 40, 0x00A22F5C, "%02u:%02u:%02u\n", dt.hour,
             dt.min, dt.sec);
    gfx_strf(&g_desktop, g_desktop.width - FONT_GLYPH_WIDTH * 12,
             g_desktop.height - 20, 0x00A22F5C, time, "%04u-%02u-%02u", dt.year,
             dt.month, dt.day);
  }
}

static window_handle g_drag_handle = (window_handle)-1;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;

static void handle_input() {
  mouse_event ev;
  if (sys_read_mouse(&ev)) {
    compositor_set_cursor(ev.x, ev.y);
    bool left_down = (ev.buttons & 1) && !(g_mouse.buttons & 1);
    bool left_up = !(ev.buttons & 1) && (g_mouse.buttons & 1);
    g_mouse = ev;

    if (left_up)
      g_drag_handle = (window_handle)-1;

    // Continue drag
    if (g_drag_handle != (window_handle)-1 && (ev.buttons & 1)) {
      window *w = &g_windows[g_drag_handle];
      w->prev_x = w->x;
      w->prev_y = w->y;
      w->x = ev.x - g_drag_off_x;
      w->y = ev.y - g_drag_off_y;
      w->moved = true;
      return;
    }

    // Update hover state for all windows
    for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
      window *w = &g_windows[i];
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

    window_handle wh = get_window_at_pos(ev.x, ev.y);
    if (wh != (window_handle)-1) {
      window *w = &g_windows[wh];

      if (left_down) {
        window_focus(w);
        if (hit_minimize(w, ev.x, ev.y)) {
          window_show(w, false);
        } else if (hit_close(w, ev.x, ev.y)) {
          // TODO: send WET_CLOSE to client
        } else if (hit_title_bar(w, ev.x, ev.y)) {
          g_drag_handle = wh;
          g_drag_off_x = ev.x - w->x;
          g_drag_off_y = ev.y - w->y;
        }
      }

      // Only forward to client if not dragging and click is in the client area
      if (g_drag_handle == (window_handle)-1 && !hit_title_bar(w, ev.x, ev.y)) {
        window_event wev = {.type = WET_MOUSE};
        wev.mouse_event.x = ev.x - w->x;
        wev.mouse_event.y = ev.y - w->y - WINDOW_TITLE_BAR_HEIGHT;
        wev.mouse_event.buttons = ev.buttons;
        wev.mouse_event.scroll = ev.scroll;
        window_post_event(w, &wev);
      }
    }
  }

  // Forward input to focused client window
  window *w = focused_window();
  if (w) {
    KeyEvent key = sys_read_key_nb();
    if (key.character || key.code) {
      window_event wev = {.type = WET_KEY_DOWN};
      wev.key_event.keycode = key.code;
      wev.key_event.character = key.character;
      window_post_event(w, &wev);
    }
    if (key.character == '0') {
      window_focus_next();
    }
  }
}

static void send_paint_events() {

  for (int i = 0; i < WINDOW_MAX_COUNT; i++) {
    window *w = &g_windows[i];
    if (!w->exists || !w->presented)
      continue;
    w->presented = false;
    window_event ev = {.type = WET_PAINT};
    ev.paint_event.paint_handle = w->usr_back_buf;
    window_post_event(w, &ev);
  }
}

int main(void) {
  sys_write("Hello from DAFNE (Desktop And File Navigation Environment)!\n");

  sys_write("Mapping fb: ");
  gfx_map(&g_screen);

  char buf[50];
  sprintf(buf, "%x\n", &g_screen);
  sys_write(buf);
  sys_write("creating surface\n");
  g_desktop = gfx_create_surface(g_screen.width, g_screen.height);

  sys_write("inniting compositor\n");
  compositor_init();

  gfx_fill(&g_desktop, BG);
  gfx_rect(&g_desktop, 0, g_desktop.height - 50, g_desktop.width, 50, TB);
  memset8((ubyte *)&g_windows, 0, sizeof(g_windows));

  sys_write("running navigator\n");
  sys_exec("/sys/programs/navigator.elf");
  sys_exec("/sys/programs/performance_monitor.elf");
  sys_exec("/sys/programs/terminal_2.elf");
  sys_read_mouse(&g_mouse);

  sys_write("running loop");
  while (1) {
    update_desktop();
    compositor_handle_events();
    handle_input();
    compositor_run();
    send_paint_events();
    sleep(7);
    // sys_yield();
  }
}
