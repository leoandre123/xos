#include "math.h"
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
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

static void layout_hstack(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  int gap = node->stack.gap;

  // compute space left for expand children
  int fixed_w = 0, expand_count = 0;
  for (ui_node *c = node->first_child; c; c = c->next_sibling) {
    if (c->expand) expand_count++;
    else fixed_w += c->preferred_size.w + gap;
  }
  if (fixed_w > 0) fixed_w -= gap;
  ushort expand_w = expand_count > 0
    ? (ushort)((size.w - fixed_w - gap * (expand_count - 1)) / expand_count)
    : 0;

  ui_pos child_pos = pos;
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    ushort cw = child->expand ? expand_w : child->preferred_size.w;
    ui_size child_size;
    switch (node->stack.align_children) {
    case ALIGN_START:
      child_pos.y = pos.y;
      child_size = (ui_size){cw, child->preferred_size.h};
      break;
    case ALIGN_END:
      child_pos.y = pos.y + size.h - child->preferred_size.h;
      child_size = (ui_size){cw, child->preferred_size.h};
      break;
    case ALIGN_CENTER:
      child_pos.y = pos.y + (size.h - child->preferred_size.h) / 2;
      child_size = (ui_size){cw, child->preferred_size.h};
      break;
    case ALIGN_STRETCH:
      child_pos.y = pos.y;
      child_size = (ui_size){cw, size.h};
      break;
    }
    child->layout(child, child_size, child_pos);
    child_pos.x += cw + gap;
  }
}

ui_node *ui_hstack(ui_node *parent, int gap, ui_align align_children) {
  ui_node *node = MAKE_NODE(parent, UI_HSTACK, ps_hstack, layout_hstack, 0);
  node->stack.gap = gap;
  node->stack.align_children = align_children;
  return node;
}