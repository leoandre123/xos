#include "retained_ui.h"
#include "fb_info.h"
#include "font.h"
#include "gfx.h"
#include "math.h"
#include "memory.h"
#include "mouse.h"
#include "string.h"
#include "types.h"
#include "window/ui/retained_ui_internal.h"
#include "window_event.h"

ui_node g_root = {0};
fb_info *fb;

ui_node g_nodes[100];
int next_idx = 0;

static ui_size ps_root(ui_node *node) {
  return WITH_PADDING(0, 0, node->padding);
}
static ui_size ps_button(ui_node *node) {
  ushort w = strlen(node->button.text) * FONT_GLYPH_WIDTH;
  ushort h = FONT_GLYPH_HEIGHT;
  return WITH_PADDING(w, h, node->padding);
}
static ui_size ps_label(ui_node *node) {
  ushort w = strlen(node->label.text) * FONT_GLYPH_WIDTH;
  ushort h = FONT_GLYPH_HEIGHT;
  return WITH_PADDING(w, h, node->padding);
}
static ui_size ps_vstack(ui_node *node) {
  ushort max_w = 0;
  ushort h = 0;

  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    max_w = MAX(max_w, child->preferred_size.w);
    h += child->preferred_size.h + node->stack.gap;
  }
  if (h > 0)
    h -= node->stack.gap;

  return WITH_PADDING(max_w, h, node->padding);
}
static ui_size ps_hstack(ui_node *node) {
  ushort max_h = 0;
  ushort w = 0;

  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    max_h = MAX(max_h, child->preferred_size.h);
    w += child->preferred_size.w + node->stack.gap;
  }
  if (w > 0)
    w -= node->stack.gap;

  return WITH_PADDING(w, max_h, node->padding);
}
static ui_size ps_grid(ui_node *node) {
  int cols = node->grid.cols;
  int gap = node->grid.gap;

  ushort col_widths[32] = {0};
  ushort row_heights[32] = {0};

  int i = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, i++) {
    int col = i % cols;
    int row = i / cols;
    col_widths[col] = MAX(col_widths[col], child->preferred_size.w);
    row_heights[row] = MAX(row_heights[row], child->preferred_size.h);
  }

  int rows = (i + cols - 1) / cols;
  ushort w = 0, h = 0;
  for (int c = 0; c < cols; c++)
    w += col_widths[c] + gap;
  for (int r = 0; r < rows; r++)
    h += row_heights[r] + gap;

  return WITH_PADDING(w > 0 ? w - gap : 0, h > 0 ? h - gap : 0, node->padding);
}
static ui_size ps_img(ui_node *node) {
  return WITH_PADDING(node->img.img->width, node->img.img->height,
                      node->padding);
}

static void layout_root(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;
  if (node->first_child) {
    node->first_child->layout(node->first_child, size, pos);
  }
}

static void layout_vstack(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  ui_pos child_pos = pos;
  ui_size child_size = size;

  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    switch (node->stack.align_children) {
    case ALIGN_START:
      child_pos.x = pos.x;
      child_size = child->preferred_size;
      break;
    case ALIGN_END:
      child_pos.x = pos.x + size.w - child->preferred_size.w;
      child_size = child->preferred_size;
      break;
    case ALIGN_CENTER:
      child_pos.x = pos.x + (size.w - child->preferred_size.w) / 2;
      child_size = child->preferred_size;
      break;
    case ALIGN_STRETCH:
      child_pos.x = pos.x;
      child_size.w = size.w;
      child_size.h = child->preferred_size.h;
      break;
    }
    child->layout(child, child_size, child_pos);
    child_pos.y += child->preferred_size.h + node->stack.gap;
  }
}
static void layout_hstack(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;
}
static void layout_grid(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  int cols = node->grid.cols;
  int gap = node->grid.gap;

  ushort col_widths[32] = {0};
  ushort row_heights[32] = {0};

  int i = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, i++) {
    col_widths[i % cols] = MAX(col_widths[i % cols], child->preferred_size.w);
    row_heights[i / cols] = MAX(row_heights[i / cols], child->preferred_size.h);
  }

  i = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, i++) {
    int col = i % cols;
    int row = i / cols;

    ui_pos child_pos = pos;
    for (int c = 0; c < col; c++)
      child_pos.x += col_widths[c] + gap;
    for (int r = 0; r < row; r++)
      child_pos.y += row_heights[r] + gap;

    child->layout(child, (ui_size){col_widths[col], row_heights[row]},
                  child_pos);
  }
}
static void draw_node_bg(ui_node *node) {
  if (node->bg_color)
    gfx_rect(fb, node->calculated_pos.x, node->calculated_pos.y,
             node->calculated_size.w, node->calculated_size.h,
             node->hovered ? node->bg_hover : node->bg_color);
}

static inline void draw_node(ui_node *node) {
  draw_node_bg(node);
  node->draw(node);
}

static void draw_root(ui_node *node) {
  if (node->first_child)
    draw_node(node->first_child);
}
static void draw_button(ui_node *node) {
  gfx_rect(fb, node->calculated_pos.x, node->calculated_pos.y,
           node->calculated_size.w, node->calculated_size.h,
           node->hovered ? node->bg_hover : node->bg_color);
  gfx_str(fb, node->calculated_pos.x + node->padding,
          node->calculated_pos.y + node->padding, node->button.text,
          node->button.color);
}

static void draw_label(ui_node *node) {
  gfx_str(fb, node->calculated_pos.x + node->padding,
          node->calculated_pos.y + node->padding, node->label.text,
          node->label.color);
}

static void draw_grid(ui_node *node) {
  for (ui_node *child = node->first_child; child; child = child->next_sibling)
    draw_node(child);
}

static void draw_stack(ui_node *node) {
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    draw_node(child);
  }
}
static void draw_img(ui_node *node) {
  gfx_img(fb, node->calculated_pos.x, node->calculated_pos.y, node->img.img);
}

ui_node *ui_create_root(fb_info *f) {
  fb = f;
  g_root.parent = 0;
  g_root.visible = true;
  g_root.type = UI_ROOT;
  g_root.draw = draw_root;
  g_root.layout = layout_root;
  g_root.get_preferred_size = ps_root;
  return &g_root;
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

ui_node *ui_vstack(ui_node *parent, int gap, ui_align align_children) {
  ui_node *node = create_node(parent);
  node->type = UI_VSTACK;
  node->get_preferred_size = ps_vstack;
  node->layout = layout_vstack;
  node->draw = draw_stack;
  node->stack.gap = gap;
  node->stack.align_children = align_children;
  return node;
}
ui_node *ui_hstack(ui_node *parent, int gap) {
  ui_node *node = create_node(parent);
  node->type = UI_HSTACK;
  node->get_preferred_size = ps_hstack;
  node->layout = layout_hstack;
  node->draw = draw_stack;
  node->stack.gap = gap;
  return node;
}
ui_node *ui_grid(ui_node *parent, int cols, int rows, int gap) {
  ui_node *node = create_node(parent);
  node->type = UI_GRID;
  node->get_preferred_size = ps_grid;
  node->layout = layout_grid;
  node->draw = draw_grid;
  node->grid.cols = cols;
  node->grid.rows = rows;
  node->grid.gap = gap;
  return node;
}

ui_node *ui_label(ui_node *parent, const char *text) {
  ui_node *node = create_node(parent);
  node->type = UI_LABEL;
  node->get_preferred_size = ps_label;
  node->layout = layout_simple;
  node->draw = draw_label;
  memcpy(node->label.text, text, strlen(text));
  return node;
}
ui_node *ui_img(ui_node *parent, bitmap *img) {
  ui_node *node = create_node(parent);
  node->type = UI_IMG;
  node->get_preferred_size = ps_img;
  node->layout = layout_simple;
  node->draw = draw_img;
  node->img.img = img;
  return node;
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

void ui_render(ui_size screen_size) {
  measure_node(&g_root);
  g_root.layout(&g_root, screen_size, (ui_pos){0, 0});
  draw_node(&g_root);
}

static void reset_hover(ui_node *node) {
  node->hovered = false;
  node->pressed = false;
  for (ui_node *child = node->first_child; child; child = child->next_sibling)
    reset_hover(child);
}

void ui_update(window_mouse_event ev) {
  reset_hover(&g_root);
  ui_node *node = hit_test(&g_root, ev.x, ev.y);

  if (node) {
    node->hovered = true;
    if (ev.buttons)
      node->pressed = true;
  }
}