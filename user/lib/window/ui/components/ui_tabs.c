#include "font.h"
#include "gfx.h"
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
#include <string.h>

#define SCROLL_BAR_SIZE 8
#define SCROLL_SPEED 4

static ui_size get_preferred_size_tabs(ui_node *node) {
  return (ui_size){0, 0};
}
static ui_size get_preferred_size_tab(ui_node *node) { return (ui_size){0, 0}; }

static void layout_tabs(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  ui_size child_size = {size.w, size.h - FONT_GLYPH_HEIGHT};
  ui_pos child_pos = {pos.x, pos.y + FONT_GLYPH_HEIGHT};
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    LAYOUT_NODE(child, child_size, child_pos);
  }
}
static void layout_tab(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  if (node->first_child) {
    LAYOUT_NODE(node->first_child, size, pos);
  }
}

static void draw(ui_node *node) {
  int i = 0;
  int offset_x = 0;
  ui_node *active_child = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, i++) {
    uint bg;
    uint fg = RGB(0, 0, 0);
    int title_len = strlen(child->tab.title);
    if (i == node->tabs.index) {
      bg = RGB(210, 210, 120);
      active_child = child;
    } else {
      if (node->hovered &&
          PT_IN_RECT(node->mouse_x, node->mouse_y,
                     node->calculated_pos.x + offset_x, node->calculated_pos.y,
                     title_len * FONT_GLYPH_WIDTH, FONT_GLYPH_HEIGHT)) {
        bg = RGB(150, 150, 150);
      } else {
        bg = RGB(120, 120, 120);
      }
    }
    gfx_rect(&g_ui_current_fb, node->calculated_pos.x + offset_x,
             node->calculated_pos.y, title_len * FONT_GLYPH_WIDTH,
             FONT_GLYPH_HEIGHT, bg);
    gfx_str(&g_ui_current_fb, node->calculated_pos.x + offset_x,
            node->calculated_pos.y, child->tab.title, fg);

    offset_x += title_len * FONT_GLYPH_WIDTH;
  }

  if (active_child) {
    draw_node(active_child);
  }
}

void on_click(ui_node *node) {
  if (node->mouse_y >= 0 && node->mouse_y < FONT_GLYPH_HEIGHT &&
      node->mouse_x >= 0) {
    int i = 0;
    int offset_x = 0;
    for (ui_node *child = node->first_child; child;
         child = child->next_sibling, i++) {
      int title_len = strlen(child->tab.title);

      if (node->mouse_x < offset_x + title_len * FONT_GLYPH_WIDTH) {
        node->tabs.index = i;
        ui_mark_dirty(node);
        return;
      }

      offset_x += title_len * FONT_GLYPH_WIDTH;
    }
  }
}

ui_node *ui_tabs(ui_node *parent) {
  ui_node *node =
      MAKE_NODE(parent, UI_TABS, get_preferred_size_tabs, layout_tabs, draw);
  node->draws_children = true;
  node->interactive = true;
  node->tabs.index = 0;
  node->_on_click = on_click;
  return node;
}

ui_node *ui_tab(ui_node *parent, const char *title) {
  ui_node *node =
      MAKE_NODE(parent, UI_TABS, get_preferred_size_tab, layout_tab, 0);
  strcpy(node->tab.title, title);

  return node;
}