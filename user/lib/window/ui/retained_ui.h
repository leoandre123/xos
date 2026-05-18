#pragma once
#include "cdefs.h"

#include "fb_info.h"
#include "image.h"
#include "keyboard.h"
#include "types.h"
#include "window_event.h"

EXTERN_C_BEGIN

#define TEXT_FIELD_MAX_LENGTH 128

typedef enum : ubyte {
  UI_ROOT,
  UI_BUTTON,
  UI_LABEL,
  UI_VSTACK,
  UI_HSTACK,
  UI_GRID,
  UI_IMG,
  UI_TEXT_FIELD,
  UI_SCROLL_CONTAINER,
  UI_TABS,
  UI_TAB,
  UI_PIE_CHART
} ui_node_type;

typedef enum : ubyte {
  ALIGN_START,
  ALIGN_END,
  ALIGN_CENTER,
  ALIGN_STRETCH
} ui_align;

typedef enum : ubyte {
  GRID_SIZING_FIT_CONTENT, //
  GRID_SIZING_FIXED,       //
  GRID_SIZING_EXPAND       //
} ui_grid_sizing;
typedef enum {
  SCROLL_VERTICAL,
  SCROLL_HORIZONTAL,
  SCROLL_BOTH
} ui_scroll_direction;

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
  short x;
  short y;
} ui_pos;

struct ui_node;

typedef struct ui_node {
  // bool dirty;
  ui_node_type type;
  union {
    struct {
      ui_pos calculated_pos;
      ui_size calculated_size;
    };
    rect calculated_rect;
  };
  ui_size preferred_size;
  struct ui_node *parent;
  struct ui_node *first_child;
  struct ui_node *next_sibling;

  void (*update)(struct ui_node *node);
  ui_size (*get_preferred_size)(struct ui_node *node);
  void (*layout)(struct ui_node *node, ui_size size, ui_pos pos);
  void (*draw)(struct ui_node *node);

  /* COMMON PROPERTIES */

  bool visible;
  ushort padding;
  uint bg_color;
  uint bg_hover;

  bool interactive;
  bool hovered;
  bool pressed;
  bool focused;
  bool expand;
  bool draws_children;

  void (*on_click)(struct ui_node *node);
  void (*_on_click)(struct ui_node *node);

  void (*on_scroll)(struct ui_node *node, int delta);
  void (*_on_scroll)(struct ui_node *node, int delta);

  void (*on_key)(struct ui_node *node, KeyEvent ev);
  void (*_on_key)(struct ui_node *node, KeyEvent ev);

  ushort mouse_x;
  ushort mouse_y;

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
      uint row_alt_color;
      uint row_hover_color;
      uint header_color;
      ushort col_widths[32];
      ui_grid_sizing col_sizing[32];
      ushort row_heights[32];
      void (*on_row_click)(struct ui_node *grid, int row);
      int hovered_row;
    } grid;
    struct {
      ui_scroll_direction direction;
      int scroll_x;
      int scroll_y;
    } scroll_container;
    struct {
      bitmap *img;
    } img;
    struct {
      char text[TEXT_FIELD_MAX_LENGTH];
    } text_field;
    struct {
      int index;
    } tabs;
    struct {
      char title[32];
    } tab;
    struct {
      int values[8];
      uint colors[8];
      int count;
    } pie_chart;
  };
} ui_node;

#define PIE_CHART_MAX_SLICES 8

typedef struct {
  int value;
  uint color;
} pie_slice;

typedef void (*btn_callback)();

void ui_init(int width, int height, int pitch);
void ui_update(window_event ev);
void ui_render(window_paint_handle handle);
void ui_set_perf(bool on);
void ui_set_debug_dirty(bool on);

void ui_mark_dirty(ui_node *node);

ui_node *ui_create_root();

/*
 * Containers
 */

ui_node *ui_container(ui_node *parent);
ui_node *ui_vstack(ui_node *parent, int gap, ui_align align_children);
ui_node *ui_hstack(ui_node *parent, int gap, ui_align align_children);

// GRID
ui_node *ui_grid(ui_node *parent, int cols, int rows, int gap);
void ui_grid_set_col_widths(ui_node *node, ...);
void ui_grid_set_col_sizing(ui_node *node, ...);

// Scroll container
ui_node *ui_scroll_container(ui_node *parent, ui_scroll_direction direction);

// TABS
ui_node *ui_tabs(ui_node *parent);
ui_node *ui_tab(ui_node *parent, const char *title);

/*
 * Leafs
 */

// Button
ui_node *rui_button(ui_node *parent, const char *title);

// Label
ui_node *ui_label(ui_node *parent, const char *text);
void ui_label_set_text(ui_node *node, const char *text);

// Image
ui_node *ui_img(ui_node *parent, bitmap *img);
void ui_img_set_img(ui_node *node, bitmap *img);

// Text field
ui_node *ui_text_field(ui_node *parent);

// Pie chart
ui_node *ui_pie_chart(ui_node *parent);
void ui_pie_chart_set_slices(ui_node *node, pie_slice *slices, int count);

EXTERN_C_END