#include "retained_ui.h"
#include "fb_info.h"
#include "gfx.h"
#include "keyboard.h"
#include "rect.h"
#include "syscall.h"
#include "types.h"
#include "window/ui/retained_ui_internal.h"
#include <string.h>

#define MAX_DIRTY_RECTS 32
static window_paint_handle *g_bufs[2] = {0};
static rect dirty_rects[2][MAX_DIRTY_RECTS];
static int dirty_rect_count[2] = {0};
bool g_full_dirty[2] = {true, true};

fb_info g_ui_current_fb = {0};
int g_ui_current_buffer_idx = 0;

rect g_current_dirty_rect = {0};
ui_node g_root = {0};
ui_node g_nodes[256];
int next_idx = 0;
ui_node *g_focused = 0;
ulong g_ui_current_time;

// ################### Debug ###################
#define DEBUG_DIRTY_RECT_FRAME_PERSIST 1
#define DEBUG_MAX_CLEAR_RECTS 64

static bool debug_show_dirty = false;
static rect debug_clear_rects[DEBUG_MAX_CLEAR_RECTS] = {0};
static int debug_clear_rects_timers[DEBUG_MAX_CLEAR_RECTS] = {0};

static bool debug_show_perf = false;
static int debug_fps = 0;
static int debug_frame_count = 0;
static ulong debug_last_sec = 0;

// ##############################################

static bool is_initialized = false;

static inline ulong rdtsc(void) {
  ulong lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return (hi << 32) | lo;
}

static int get_fb_index(window_paint_handle handle) {
  if (handle == g_bufs[0])
    return 0;
  if (handle == g_bufs[1])
    return 1;

  int idx = g_bufs[0] ? 1 : 0;
  g_bufs[idx] = handle;
  return idx;
}

static ui_size ps_root(ui_node *node) {
  return WITH_PADDING(0, 0, node->padding);
}

static void layout_root(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;
  if (node->first_child) {
    node->first_child->layout(node->first_child, size, pos);
  }
}

ui_node *create_node(ui_node *parent) {
  if (next_idx >= (int)(sizeof(g_nodes) / sizeof(g_nodes[0]))) {
    sys_write("UI: node pool exhausted\n");
    for (;;)
      sys_yield();
  }
  ui_node *node = &g_nodes[next_idx++];
  node->parent = parent;
  node->visible = true;
  // node->dirty = true;
  node->expand = false;
  node->interactive = false;
  node->first_child = 0;
  node->next_sibling = 0;

  if (parent->first_child) {
    ui_node *child;
    for (child = parent->first_child; child->next_sibling;
         child = child->next_sibling) {
    }
    child->next_sibling = node;
  } else {
    parent->first_child = node;
  }

  return node;
}

ui_node *ui_create_root() {
  g_root.parent = 0;
  g_root.visible = true;
  g_root.type = UI_ROOT;
  g_root.draw = 0;
  g_root.layout = layout_root;
  g_root.get_preferred_size = ps_root;
  return &g_root;
}

static void measure_node(ui_node *node) {
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    measure_node(child);
  }

  node->preferred_size = node->get_preferred_size(node);
}

static ui_node *hit_test(ui_node *node, int mx, int my) {
  if (!node->visible)
    return 0;

  if (mx >= node->calculated_pos.x &&
      mx < node->calculated_pos.x + node->calculated_size.w &&
      my >= node->calculated_pos.y &&
      my < node->calculated_pos.y + node->calculated_size.h) {
    for (ui_node *child = node->first_child; child;
         child = child->next_sibling) {
      ui_node *hit = hit_test(child, mx, my);
      if (hit)
        return hit;
    }
    if (node->interactive)
      return node;
  }
  return 0;
}

static ui_node *find_scrolling_node(ui_node *node) {
  if (node->type == UI_SCROLL_CONTAINER)
    return node;

  if (node->parent)
    return find_scrolling_node(node->parent);
  return 0;
}

static void debug_clear_dirty_rects() {
  // for (int i = 0; i < DEBUG_MAX_CLEAR_RECTS; i++) {
  //   if (debug_clear_rects_timers[i]) {
  //     if (!--debug_clear_rects_timers[i]) {
  //       gfx_set_clip(fb, debug_clear_rects[i].x, debug_clear_rects[i].y,
  //                    debug_clear_rects[i].w, debug_clear_rects[i].h);
  //       g_current_dirty_rect = debug_clear_rects[i];
  //       draw_node(&g_root);
  //     }
  //   }
  // }
  // gfx_clear_clip(fb);
}

static void debug_add_dirty_rect(rect rc) {
  // for (int i = 0; i < DEBUG_MAX_CLEAR_RECTS; i++) {
  //   if (!debug_clear_rects_timers[i]) {
  //     debug_clear_rects[i] = rc;
  //     debug_clear_rects_timers[i] = DEBUG_DIRTY_RECT_FRAME_PERSIST;
  //     return;
  //   }
  // }
}

static int debug_dirty_rec_count() {
  // int c = 0;
  // for (int i = 0; i < DEBUG_MAX_CLEAR_RECTS; i++) {
  //   if (debug_clear_rects_timers[i]) {
  //     c++;
  //   }
  // }
  //
  // return c;
}

static void debug_create_dirty_rects() {
  // gfx_clear_clip(fb);
  // if (g_full_dirty) {
  //   gfx_rect(fb, 0, 0, fb->width, fb->height, ARGB(60, 255, 0, 0));
  //   debug_add_dirty_rect((rect){0, 0, fb->width, fb->height});
  // } else {
  //   for (int i = 0; i < dirty_rect_count; i++) {
  //     gfx_rect(fb, dirty_rects[i].x, dirty_rects[i].y, dirty_rects[i].w,
  //              dirty_rects[i].h, ARGB(60, 255, 0, 0));
  //     debug_add_dirty_rect(dirty_rects[i]);
  //   }
  // }
}

static void ui_draw() {

  if (debug_show_dirty) {
    debug_clear_dirty_rects();
  }

  if (!g_full_dirty[g_ui_current_buffer_idx] &&
      dirty_rect_count[g_ui_current_buffer_idx] == 0)
    return;

  // sys_write("Render\n");
  if (g_full_dirty[g_ui_current_buffer_idx]) {
    gfx_clear_clip(&g_ui_current_fb);
    draw_node(&g_root);
  } else {
    for (int i = 0; i < dirty_rect_count[g_ui_current_buffer_idx]; i++) {
      gfx_set_clip(&g_ui_current_fb, dirty_rects[g_ui_current_buffer_idx][i]);
      g_current_dirty_rect = dirty_rects[g_ui_current_buffer_idx][i];
      draw_node(&g_root);
    }
  }

  if (debug_show_dirty) {
    debug_create_dirty_rects();
  }

  dirty_rect_count[g_ui_current_buffer_idx] = 0;
  g_full_dirty[g_ui_current_buffer_idx] = false;
  gfx_clear_clip(&g_ui_current_fb);
}

static void update_node(ui_node *node) {

  if (node->update)
    node->update(node);

  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    update_node(child);
  }
}

static void reset_hover(ui_node *node, ui_node *current_hover) {
  if (node->hovered) {
    if (node != current_hover) {
      node->hovered = false;
      ui_mark_dirty(node);
    }
  }
  node->pressed = false;
  for (ui_node *child = node->first_child; child; child = child->next_sibling)
    reset_hover(child, current_hover);
}

void ui_mark_dirty(ui_node *node) {
  if (dirty_rect_count[0] >= MAX_DIRTY_RECTS) {
    g_full_dirty[0] = true;
  } else {
    dirty_rects[0][dirty_rect_count[0]++] = node->calculated_rect;
  }
  if (dirty_rect_count[1] >= MAX_DIRTY_RECTS) {
    g_full_dirty[1] = true;
  } else {
    dirty_rects[1][dirty_rect_count[1]++] = node->calculated_rect;
  }
}

void ui_update(window_event ev) {

  switch (ev.type) {

  case WET_KEY_DOWN: {
    if (g_focused && g_focused->_on_key) {
      KeyEvent ke = {ev.key_event.keycode, ev.key_event.character};
      g_focused->_on_key(g_focused, ke);
    }
    break;
  }
  case WET_MOUSE: {
    static int prev_buttons = 0;
    int just_pressed = ev.mouse_event.buttons && !prev_buttons;
    prev_buttons = ev.mouse_event.buttons;
    ui_node *node = hit_test(&g_root, ev.mouse_event.x, ev.mouse_event.y);
    reset_hover(&g_root, node);

    if (node) {
      node->hovered = true;
      ui_mark_dirty(node);
      node->mouse_x = ev.mouse_event.x - node->calculated_pos.x;
      node->mouse_y = ev.mouse_event.y - node->calculated_pos.y;

      if (ev.mouse_event.buttons)
        node->pressed = true;
      if (just_pressed) {
        if (g_focused && g_focused != node)
          g_focused->focused = false;
        g_focused = node;
        node->focused = true;
        if (node->_on_click)
          node->_on_click(node);
        if (node->on_click)
          node->on_click(node);
      }
      if (ev.mouse_event.scroll) {
        ui_node *scroll_node = find_scrolling_node(node);
        if (scroll_node && scroll_node->_on_scroll)
          scroll_node->_on_scroll(scroll_node, ev.mouse_event.scroll);
        if (scroll_node && scroll_node->on_scroll)
          scroll_node->on_scroll(scroll_node, ev.mouse_event.scroll);
      }
    }
    break;
  }
  default: break;
  }
}
void ui_render(window_paint_handle paint_handle) {
  if (!is_initialized) {
    sys_write("ERROR: ui not initialized");
    return;
  }

  // Set buffer index
  g_ui_current_buffer_idx = get_fb_index(paint_handle);
  g_ui_current_fb.ptr = paint_handle;

  g_ui_current_time = sys_unix_time();
  update_node(&g_root);
  measure_node(&g_root);
  g_root.layout(&g_root,
                (ui_size){g_ui_current_fb.width, g_ui_current_fb.height},
                (ui_pos){0, 0});

  if (debug_show_perf) {
    int rc = debug_dirty_rec_count();
    bool was_full = g_full_dirty[g_ui_current_buffer_idx];
    ulong t0 = rdtsc();
    ui_draw();
    ulong cycles = rdtsc() - t0;

    debug_frame_count++;
    if (g_ui_current_time != debug_last_sec) {
      debug_fps = debug_frame_count;
      debug_frame_count = 0;
      debug_last_sec = g_ui_current_time;
    }

    char buf[64];
    sprintf(buf, "FPS:%4d  render:%6dK cyc rc:%2d full:%d", debug_fps,
            cycles / 1000, rc, was_full);
    gfx_rect(&g_ui_current_fb, 0, 0, strlen(buf) * FONT_GLYPH_WIDTH + 8,
             FONT_GLYPH_HEIGHT + 4, RGB(0, 0, 0));
    gfx_str(&g_ui_current_fb, 4, 2, buf, RGB(255, 255, 0));
  } else {
    ui_draw();
  }
}

void ui_init(int width, int height, int pitch) {
  g_ui_current_fb.width = width;
  g_ui_current_fb.height = height;
  g_ui_current_fb.pitch = pitch;
  g_ui_current_fb.dirty_region = (rect){0};

  is_initialized = true;
}

void ui_set_perf(bool on) { debug_show_perf = on; }
void ui_set_debug_dirty(bool on) { debug_show_dirty = on; }
