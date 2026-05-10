
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
static ui_size ps_label(ui_node *node) {
  ushort w = strlen(node->label.text) * FONT_GLYPH_WIDTH;
  ushort h = FONT_GLYPH_HEIGHT;
  return WITH_PADDING(w, h, node->padding);
}

static void draw_label(ui_node *node) {
  gfx_str(fb, node->calculated_pos.x + node->padding,
          node->calculated_pos.y + node->padding, node->label.text,
          node->label.color);
}

void ui_label_set_text(ui_node *node, const char *text) {
  strcpy(node->label.text, text);
  node->dirty = true;
  if (node->parent)
    node->parent->dirty = true;
}

ui_node *ui_label(ui_node *parent, const char *text) {
  ui_node *node =
      MAKE_NODE(parent, UI_LABEL, ps_label, layout_simple, draw_label);
  strcpy(node->label.text, text);
  return node;
}