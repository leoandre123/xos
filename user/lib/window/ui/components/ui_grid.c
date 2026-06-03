
#include "gfx.h"
#include "math.h"
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
#include <stdarg.h>

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
    col_widths[col] = MAX(col_widths[col], child->_preferred_size.w);
    row_heights[row] = MAX(row_heights[row], child->_preferred_size.h);
  }

  int rows = (i + cols - 1) / cols;
  ushort w = 0, h = 0;
  for (int c = 0; c < cols; c++)
    w += col_widths[c] + gap;
  for (int r = 0; r < rows; r++)
    h += row_heights[r] + gap;

  return WITH_PADDING(w > 0 ? w - gap : 0, h > 0 ? h - gap : 0, node->padding);
}

static void layout_grid(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  int cols = node->grid.cols;
  int gap = node->grid.gap;

  for (int i = 0; i < 32; i++)
    node->grid.row_heights[i] = 0;

  // FIT pass: measure content width for FIT columns; leave FIXED as-is
  ushort fit_w[32] = {0};
  int i = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, i++) {
    int col = i % cols, row = i / cols;
    if (node->grid.col_sizing[col] == GRID_SIZING_FIT_CONTENT)
      fit_w[col] = MAX(fit_w[col], child->_preferred_size.w);
    node->grid.row_heights[row] =
        MAX(node->grid.row_heights[row], child->_preferred_size.h);
  }
  for (int c = 0; c < cols; c++)
    if (node->grid.col_sizing[c] == GRID_SIZING_FIT_CONTENT)
      node->grid.col_widths[c] = fit_w[c];

  // FILL pass: divide remaining width equally among FILL columns
  int fill_count = 0;
  int used_w = (cols - 1) * gap;
  for (int c = 0; c < cols; c++) {
    if (node->grid.col_sizing[c] == GRID_SIZING_EXPAND)
      fill_count++;
    else
      used_w += node->grid.col_widths[c];
  }
  if (fill_count > 0) {
    int remaining = (int)size.w - used_w;
    if (remaining < 0)
      remaining = 0;
    ushort fill_each = (ushort)(remaining / fill_count);
    ushort fill_rem = (ushort)(remaining % fill_count);
    int fi = 0;
    for (int c = 0; c < cols; c++) {
      if (node->grid.col_sizing[c] == GRID_SIZING_EXPAND) {
        node->grid.col_widths[c] =
            fill_each + (++fi == fill_count ? fill_rem : 0);
      }
    }
  }

  // Position children
  i = 0;
  for (ui_node *child = node->first_child; child;
       child = child->next_sibling, i++) {
    int col = i % cols, row = i / cols;
    ui_pos child_pos = pos;
    for (int c = 0; c < col; c++)
      child_pos.x += node->grid.col_widths[c] + gap;
    for (int r = 0; r < row; r++)
      child_pos.y += node->grid.row_heights[r] + gap;
    child->layout(
        child,
        (ui_size){node->grid.col_widths[col], node->grid.row_heights[row]},
        child_pos);
  }
}

static void draw_grid(ui_node *node) {
  int child_count = get_child_count(node);
  int row_count = child_count == 0 ? 0 : (child_count / node->grid.cols) + 1;
  int gap = node->grid.gap;

  int y = node->calculated_pos.y;
  for (int row = 0; row < row_count; row++) {
    int row_height = node->grid.row_heights[row];
    uint color = 0;
    if (row == node->grid.hovered_row && node->grid.row_hover_color)
      color = node->grid.row_hover_color;
    else if (row == 0 && node->grid.header_color)
      color = node->grid.header_color;
    else if (row % 2 == 0 && node->grid.row_alt_color)
      color = node->grid.row_alt_color;
    if (color)
      gfx_rect(&g_ui_current_fb, node->calculated_pos.x, y,
               node->calculated_size.w, row_height, color);
    y += row_height + gap;
  }
}

static void on_click_grid(ui_node *node) {
  if (!node->grid.on_row_click)
    return;
  int y = 0, gap = node->grid.gap;
  int row_count = node->grid.rows;
  for (int row = 0; row < row_count; row++) {
    int rh = node->grid.row_heights[row];
    if (node->mouse_y >= y && node->mouse_y < y + rh) {
      node->grid.on_row_click(node, row);
      return;
    }
    y += rh + gap;
  }
}

static void grid_update(ui_node *node) {
  int child_count = get_child_count(node);
  int row_count = child_count == 0 ? 0 : (child_count / node->grid.cols) + 1;
  int gap = node->grid.gap;

  if (node->hovered) {
    int y = 0;
    for (int row = 0; row < row_count; row++) {
      int rh = node->grid.row_heights[row];
      if (node->mouse_y >= y && node->mouse_y < y + rh) {
        if (node->grid.hovered_row != row) {
          ui_mark_dirty(node);
          node->grid.hovered_row = row;
        }
        break;
      }
      y += rh + gap;
    }
  } else if (node->grid.hovered_row != -1) {
    node->grid.hovered_row = -1;
    ui_mark_dirty(node);
  }
}

ui_node *ui_grid(ui_node *parent, int cols, int rows, int gap) {
  ui_node *node = MAKE_NODE(parent, UI_GRID, ps_grid, layout_grid, draw_grid);
  node->interactive = true;
  node->_on_click = on_click_grid;
  node->update = grid_update;
  node->grid.cols = cols;
  node->grid.rows = rows;
  node->grid.gap = gap;
  node->grid.row_alt_color = 0;
  node->grid.row_hover_color = 0;
  node->grid.header_color = 0;
  node->grid.on_row_click = 0;
  node->grid.hovered_row = -1;
  return node;
}

void ui_grid_set_col_sizing(ui_node *node, ...) {
  va_list args;
  va_start(args, node);
  for (int i = 0; i < node->grid.cols; i++) {
    node->grid.col_sizing[i] = (ui_grid_sizing)va_arg(args, int);
  }
  va_end(args);
}
void ui_grid_set_col_widths(ui_node *node, ...) {
  va_list args;
  va_start(args, node);
  for (int i = 0; i < node->grid.cols; i++) {
    node->grid.col_widths[i] = (ushort)va_arg(args, int);
  }
  va_end(args);
}