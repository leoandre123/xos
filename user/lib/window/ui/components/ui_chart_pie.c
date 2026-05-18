#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"

// Returns 0..63 for a full clockwise circle, 0 = 3 o'clock (positive x axis)
static int atan2_64(int y, int x) {
  if (x == 0 && y == 0)
    return 0;
  int ax = x < 0 ? -x : x;
  int ay = y < 0 ? -y : y;
  int idx;
  if (ax >= ay) {
    idx = ay > 0 ? (ay * 8 + (ax >> 1)) / ax : 0;
    if (x >= 0)
      return y >= 0 ? idx : (64 - idx) & 63;
    return y >= 0 ? 32 - idx : 32 + idx;
  } else {
    idx = ax > 0 ? 16 - (ax * 8 + (ay >> 1)) / ay : 16;
    if (x >= 0)
      return y >= 0 ? idx : (64 - idx) & 63;
    return y >= 0 ? 32 - idx : 32 + idx;
  }
}

static ui_size ps_pie(ui_node *node) {
  (void)node;
  return (ui_size){100, 100};
}

static void draw_pie(ui_node *node) {
  int count = node->pie_chart.count;
  if (count <= 0)
    return;

  int cx = node->calculated_pos.x + node->calculated_size.w / 2;
  int cy = node->calculated_pos.y + node->calculated_size.h / 2;
  int r = (node->calculated_size.w < node->calculated_size.h
               ? node->calculated_size.w
               : node->calculated_size.h) /
              2 -
          1;
  if (r <= 0)
    return;

  int total = 0;
  for (int i = 0; i < count; i++)
    total += node->pie_chart.values[i];
  if (total == 0)
    return;

  // Precompute cumulative slice boundaries in 0..64 units
  int cum[PIE_CHART_MAX_SLICES + 1];
  cum[0] = 0;
  int acc = 0;
  for (int i = 0; i < count; i++) {
    acc += node->pie_chart.values[i];
    cum[i + 1] = acc * 64 / total;
  }
  cum[count] = 64;

  int r2 = r * r;
  for (int dy = -r; dy <= r; dy++) {
    for (int dx = -r; dx <= r; dx++) {
      if (dx * dx + dy * dy > r2)
        continue;

      // 0..63 clockwise from 12 o'clock (+16 rotates from 3 o'clock origin)
      int a = (atan2_64(dy, dx) + 16) & 63;

      uint color = node->pie_chart.colors[count - 1];
      for (int s = 0; s < count - 1; s++) {
        if (a >= cum[s] && a < cum[s + 1]) {
          color = node->pie_chart.colors[s];
          break;
        }
      }
      gfx_pixel(&g_ui_current_fb, cx + dx, cy + dy, color);
    }
  }
}

ui_node *ui_pie_chart(ui_node *parent) {
  return MAKE_NODE(parent, UI_PIE_CHART, ps_pie, layout_simple, draw_pie);
}

void ui_pie_chart_set_slices(ui_node *node, pie_slice *slices, int count) {
  if (count > PIE_CHART_MAX_SLICES)
    count = PIE_CHART_MAX_SLICES;
  node->pie_chart.count = count;
  for (int i = 0; i < count; i++) {
    node->pie_chart.values[i] = slices[i].value;
    node->pie_chart.colors[i] = slices[i].color;
  }
  ui_mark_dirty(node);
}
