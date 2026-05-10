#include "retained_ui.h"
#include "fb_info.h"
#include "keyboard.h"
#include "syscall.h"
#include "types.h"
#include "window/ui/retained_ui_internal.h"
#include "window_event.h"

/*
 * INCLUDE COMPONENTS
 */
#include "components/ui_button.inc"
#include "components/ui_grid.inc"
#include "components/ui_hstack.inc"
#include "components/ui_img.inc"
#include "components/ui_label.inc"
#include "components/ui_text_field.inc"
#include "components/ui_vstack.inc"

ui_node g_root = {0};
fb_info *fb;
ui_node g_nodes[100];
int next_idx = 0;
ui_node *g_focused = 0;

static bool s_perf = false;
int s_fps = 0, s_frame_count = 0, s_dirty_count = 0;
static ulong s_last_sec = 0;

static inline ulong rdtsc(void) {
  ulong lo, hi;
  __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
  return (hi << 32) | lo;
}

void ui_set_perf(bool on) { s_perf = on; }

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
  ui_node *node = &g_nodes[next_idx++];
  node->parent = parent;
  node->visible = true;
  node->dirty = true;
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

ui_node *ui_create_root(fb_info *f) {
  fb = f;
  g_root.parent = 0;
  g_root.visible = true;
  g_root.type = UI_ROOT;
  g_root.draw = 0;
  g_root.layout = layout_root;
  g_root.get_preferred_size = ps_root;
  g_root.dirty = true;
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
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    ui_node *hit = hit_test(child, mx, my);
    if (hit)
      return hit;
  }

  if (node->interactive &&
      (mx > node->calculated_pos.x &&
       mx < node->calculated_pos.x + node->calculated_size.w &&
       my > node->calculated_pos.y &&
       my < node->calculated_pos.y + node->calculated_size.h))
    return node;

  return 0;
}

static void update_node(ui_node *node, ulong time);

void ui_render(ui_size screen_size) {
  update_node(&g_root, sys_time());
  measure_node(&g_root);
  g_root.layout(&g_root, screen_size, (ui_pos){0, 0});

  if (s_perf) {
    s_dirty_count = 0;
    ulong t0 = rdtsc();
    draw_node(&g_root, false);
    ulong cycles = rdtsc() - t0;

    s_frame_count++;
    ulong now = sys_time();
    if (now != s_last_sec) {
      s_fps = s_frame_count;
      s_frame_count = 0;
      s_last_sec = now;
    }

    char buf[64];
    sprintf(buf, "FPS:%4d  render:%6dK cyc  dirty:%5d", s_fps, cycles / 1000,
            s_dirty_count);
    gfx_rect(fb, 0, 0, strlen(buf) * FONT_GLYPH_WIDTH + 8,
             FONT_GLYPH_HEIGHT + 4, RGB(0, 0, 0));
    gfx_str(fb, 4, 2, buf, RGB(255, 255, 0));
  } else {
    draw_node(&g_root, false);
  }
}

static void reset_hover(ui_node *node, ui_node *current_hover) {
  if (node->hovered) {
    if (node != current_hover) {
      node->hovered = false;
      node->dirty = true;
    }
  }
  node->pressed = false;
  for (ui_node *child = node->first_child; child; child = child->next_sibling)
    reset_hover(child, current_hover);
}

void ui_mark_dirty(ui_node *node) { node->dirty = true; }

static void update_node(ui_node *node, ulong time) {

  if (node->update)
    node->update(node, time);

  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    update_node(child, time);
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
      if (!node->hovered)
        node->dirty = true;
      node->hovered = true;
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
    }
    break;
  }
  default: break;
  }
}