#include "gfx.h"
#include "math.h"
#include "rect.h"
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"

#define SCROLL_BAR_SIZE 8
#define SCROLL_SPEED 4

static ui_size get_preferred_size(ui_node *node) { return (ui_size){0, 0}; }

static void layout(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  if (node->first_child) {
    ui_size pref_size = node->first_child->_preferred_size;
    ui_pos child_pos = (ui_pos){pos.x - node->scroll_container.scroll_x,
                                pos.y - node->scroll_container.scroll_y};
    switch (node->scroll_container.direction) {
    case SCROLL_VERTICAL:
      LAYOUT_NODE(node->first_child,
                  ((ui_size){size.w - SCROLL_BAR_SIZE, pref_size.h}),
                  child_pos);
      break;
    case SCROLL_HORIZONTAL:
      LAYOUT_NODE(node->first_child,
                  ((ui_size){pref_size.w, size.h - SCROLL_BAR_SIZE}),
                  child_pos);
      break;
    case SCROLL_BOTH:
      LAYOUT_NODE(
          node->first_child,
          ((ui_size){size.w - SCROLL_BAR_SIZE, size.h - SCROLL_BAR_SIZE}),
          child_pos);
      break;
    }
  }
}

static void draw(ui_node *node) {

  float percentage = node->first_child
                         ? ((float)node->calculated_size.h) /
                               (float)node->first_child->calculated_size.h
                         : 0;

  if (node->scroll_container.direction == SCROLL_VERTICAL ||
      node->scroll_container.direction == SCROLL_BOTH) {
    gfx_rect(&g_ui_current_fb,
             node->calculated_pos.x + node->calculated_size.w - SCROLL_BAR_SIZE,
             node->calculated_pos.y, SCROLL_BAR_SIZE, node->calculated_size.h,
             RGB(180, 180, 180));
    gfx_rect(&g_ui_current_fb,
             node->calculated_pos.x + node->calculated_size.w - SCROLL_BAR_SIZE,
             node->calculated_pos.y +
                 node->scroll_container.scroll_y * percentage,
             SCROLL_BAR_SIZE, node->calculated_size.h * percentage,
             RGB(120, 120, 120));
  }

  if (node->scroll_container.direction == SCROLL_HORIZONTAL ||
      node->scroll_container.direction == SCROLL_BOTH) {
    gfx_rect(&g_ui_current_fb, node->calculated_pos.x,
             node->calculated_pos.y + node->calculated_size.h - SCROLL_BAR_SIZE,
             node->calculated_size.w, SCROLL_BAR_SIZE, RGB(180, 180, 180));
  }

  rect clip_before = gfx_get_clip();
  rect intersection = rect_intersect(g_clip_rect, node->calculated_rect);
  gfx_set_clip(&g_ui_current_fb, intersection);
  if (node->first_child) {
    draw_node(node->first_child);
  }
  gfx_set_clip(&g_ui_current_fb, clip_before);
}

static void on_scroll(ui_node *node, int delta) {
  int max_scroll = node->first_child
                       ? (float)node->first_child->calculated_size.h -
                             ((float)node->calculated_size.h)
                       : 0;
  max_scroll = MAX(max_scroll, 0);

  node->scroll_container.scroll_y = CLAMP(
      node->scroll_container.scroll_y + delta * SCROLL_SPEED, 0, max_scroll);
  ui_mark_dirty(node);
}

ui_node *ui_scroll_container(ui_node *parent, ui_scroll_direction direction) {
  ui_node *node =
      MAKE_NODE(parent, UI_SCROLL_CONTAINER, get_preferred_size, layout, draw);
  node->scroll_container.direction = direction;
  node->draws_children = true;
  node->_on_scroll = on_scroll;
  return node;
}