#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
static ui_size ps_img(ui_node *node) {
  if (!node->img.img) {
    return WITH_PADDING(0, 0, node->padding);
  }
  return WITH_PADDING(node->img.img->width, node->img.img->height,
                      node->padding);
}
static void draw_img(ui_node *node) {
  if (node->img.img)
    gfx_img(fb, node->calculated_pos.x, node->calculated_pos.y, node->img.img);
}

void ui_img_set_img(ui_node *node, bitmap *img) {
  node->img.img = img;
  node->dirty = true;
}

ui_node *ui_img(ui_node *parent, bitmap *img) {
  ui_node *node = MAKE_NODE(parent, UI_IMG, ps_img, layout_simple, draw_img);
  node->img.img = img;
  return node;
}
