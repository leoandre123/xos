#pragma once
#include "cdefs.h"

#include "fb_info.h"
#include "gfx.h"
#include "rect.h"
#include "window/ui/retained_ui.h"

EXTERN_C_BEGIN

extern fb_info g_ui_current_fb;
extern int g_ui_current_buffer_idx;

extern bool g_full_dirty[2];
extern rect g_current_dirty_rect;
extern ulong g_ui_current_time;

#define WITH_PADDING(width, height, p)                                         \
  ((ui_size){.w = width + p + p, .h = height + p + p})

#define MAKE_NODE(p, t, ps, ly, drw)                                           \
  ({                                                                           \
    ui_node *_node = create_node(p);                                           \
    _node->type = t;                                                           \
    _node->get_preferred_size = ps;                                            \
    _node->layout = ly;                                                        \
    _node->draw = drw;                                                         \
    _node;                                                                     \
  })

#define LAYOUT_NODE(n, s, p) n->layout(n, s, p)

#define PT_IN_RECT(px, py, rx, ry, rw, rh)                                     \
  (px >= rx && px < rx + rw && py >= ry && py < ry + rh)

static void layout_simple(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;
}

// extern int s_dirty_count;

static void draw_node_bg(ui_node *node) {
  uint color =
      (node->hovered && node->bg_hover) ? node->bg_hover : node->bg_color;
  if (color)
    gfx_rect(&g_ui_current_fb, node->calculated_pos.x, node->calculated_pos.y,
             node->calculated_size.w, node->calculated_size.h, color);
}

static inline bool node_intersects_dirty(ui_node *node) {
  return g_full_dirty[g_ui_current_buffer_idx]
             ? true
             : rect_intersect(node->calculated_rect, g_current_dirty_rect).w;
}

static inline void draw_node(ui_node *node) {
  if (!node_intersects_dirty(node))
    return;
  // s_dirty_count++;
  draw_node_bg(node);
  if (node->draw)
    node->draw(node);

  if (!node->draws_children) {
    for (ui_node *child = node->first_child; child;
         child = child->next_sibling) {
      draw_node(child);
    }
  }
}

// static inline void draw_node(ui_node *node) { draw_node_inner(node, false); }

static inline int get_child_count(ui_node *node) {
  int c = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, c++)
    ;
  return c;
}

extern ui_node *create_node(ui_node *parent);

EXTERN_C_END