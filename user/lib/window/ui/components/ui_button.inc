#include "memory.h"
#include "window/ui/retained_ui_internal.h"

static ui_size ps_button(ui_node *node) {
  ushort w = strlen(node->button.text) * FONT_GLYPH_WIDTH;
  ushort h = FONT_GLYPH_HEIGHT;
  return WITH_PADDING(w, h, node->padding);
}

static void draw_button(ui_node *node) {
  gfx_rect(fb, node->calculated_pos.x, node->calculated_pos.y,
           node->calculated_size.w, node->calculated_size.h,
           node->hovered ? node->bg_hover : node->bg_color);
  gfx_str(fb, node->calculated_pos.x + node->padding,
          node->calculated_pos.y + node->padding, node->button.text,
          node->button.color);
}

ui_node *rui_button(ui_node *parent, const char *title) {
  ui_node *node = create_node(parent);
  node->type = UI_BUTTON;
  node->get_preferred_size = ps_button;
  node->layout = layout_simple;
  node->draw = draw_button;
  node->padding = 5;
  node->interactive = true;
  node->bg_color = 0xff505080;
  node->bg_hover = 0xff303050;
  node->button.color = 0x00ffffff;
  memcpy(node->button.text, title, strlen(title));
  return node;
}