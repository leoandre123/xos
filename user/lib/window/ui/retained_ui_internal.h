#pragma once
#include "cdefs.h"

#include "fb_info.h"
#include "font.h"
#include "gfx.h"
#include "window/ui/retained_ui.h"
#include <string.h>

EXTERN_C_BEGIN

extern fb_info *fb;

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

static void layout_simple(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;
}

extern int s_dirty_count;

static void draw_node_bg(ui_node *node) {
  uint color = (node->hovered && node->bg_hover) ? node->bg_hover : node->bg_color;
  if (color)
    gfx_rect(fb, node->calculated_pos.x, node->calculated_pos.y,
             node->calculated_size.w, node->calculated_size.h, color);
}

static inline void draw_node(ui_node *node, bool parent_dirty) {
  bool should_draw = node->dirty || parent_dirty;
  if (should_draw) {
    s_dirty_count++;
    draw_node_bg(node);
    if (node->draw)
      node->draw(node);
    node->dirty = false;
  }
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    draw_node(child, should_draw);
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