#include "math.h"
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
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

static void layout_vstack(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;

  int gap = node->stack.gap;

  // compute space left for expand children
  int fixed_h = 0, expand_count = 0;
  for (ui_node *c = node->first_child; c; c = c->next_sibling) {
    if (c->expand) expand_count++;
    else fixed_h += c->preferred_size.h + gap;
  }
  if (fixed_h > 0) fixed_h -= gap;
  ushort expand_h = expand_count > 0
    ? (ushort)((size.h - fixed_h - gap * (expand_count - 1)) / expand_count)
    : 0;

  ui_pos child_pos = pos;
  for (ui_node *child = node->first_child; child; child = child->next_sibling) {
    ushort ch = child->expand ? expand_h : child->preferred_size.h;
    ui_size child_size;
    switch (node->stack.align_children) {
    case ALIGN_START:
      child_pos.x = pos.x;
      child_size = (ui_size){child->preferred_size.w, ch};
      break;
    case ALIGN_END:
      child_pos.x = pos.x + size.w - child->preferred_size.w;
      child_size = (ui_size){child->preferred_size.w, ch};
      break;
    case ALIGN_CENTER:
      child_pos.x = pos.x + (size.w - child->preferred_size.w) / 2;
      child_size = (ui_size){child->preferred_size.w, ch};
      break;
    case ALIGN_STRETCH:
      child_pos.x = pos.x;
      child_size = (ui_size){size.w, ch};
      break;
    }
    child->layout(child, child_size, child_pos);
    child_pos.y += ch + gap;
  }
}

ui_node *ui_vstack(ui_node *parent, int gap, ui_align align_children) {
  ui_node *node = MAKE_NODE(parent, UI_VSTACK, ps_vstack, layout_vstack, 0);
  node->stack.gap = gap;
  node->stack.align_children = align_children;
  return node;
}