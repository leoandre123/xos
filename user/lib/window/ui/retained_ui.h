#pragma once

#include "fb_info.h"
#include "image.h"
#include "mouse.h"
#include "types.h"
#include "window_event.h"
typedef enum : ubyte {
  UI_ROOT,
  UI_BUTTON,
  UI_LABEL,
  UI_VSTACK,
  UI_HSTACK,
  UI_GRID,
  UI_IMG
} ui_node_type;

typedef enum : ubyte {
  ALIGN_START,
  ALIGN_END,
  ALIGN_CENTER,
  ALIGN_STRETCH
} ui_align;

typedef struct {
  ushort min_w;
  ushort min_h;
  ushort max_w;
  ushort max_h;
} ui_constraints;

typedef struct {
  ushort w;
  ushort h;
} ui_size;

typedef struct {
  ushort x;
  ushort y;
} ui_pos;

struct ui_node;

typedef ui_size (*ui_get_preferred_size)(struct ui_node *node);
typedef void (*ui_do_layout)(struct ui_node *node, ui_size size, ui_pos pos);
typedef void (*ui_draw_node)(struct ui_node *node);

typedef struct ui_node {
  bool dirty;
  ui_node_type type;
  ui_pos calculated_pos;
  ui_size calculated_size;
  ui_size preferred_size;

  struct ui_node *parent;
  struct ui_node *first_child;
  struct ui_node *next_sibling;

  ui_get_preferred_size get_preferred_size;
  ui_do_layout layout;
  ui_draw_node draw;

  /* COMMON PROPERTIES */

  bool visible;
  ushort padding;
  uint bg_color;
  uint bg_hover;

  bool interactive;
  bool hovered;
  bool pressed;

  /* TYPE SPECIFIC */
  union {
    struct {
      char text[64];
      uint color;
    } label;
    struct {
      char text[64];
      uint color;
    } button;
    struct {
      ui_align align_children;
      int gap;
    } stack;
    struct {
      int cols;
      int rows;
      int gap;
    } grid;
    struct {
      bitmap *img;
    } img;
  };
} ui_node;

typedef void (*btn_callback)();

void ui_update(window_mouse_event ev);
void ui_draw();

ui_node *ui_create_root(fb_info *fb);
ui_node *ui_container(ui_node *parent);
ui_node *ui_vstack(ui_node *parent, int gap, ui_align align_children);
ui_node *ui_hstack(ui_node *parent, int gap);
ui_node *ui_grid(ui_node *parent, int cols, int rows, int gap);

ui_node *rui_button(ui_node *parent, const char *title);
ui_node *ui_label(ui_node *parent, const char *text);
ui_node *ui_img(ui_node *parent, bitmap *img);

void ui_render(ui_size screen_size);