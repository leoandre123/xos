#pragma once

#include "fb_info.h"
#include "font.h"
#include "gfx.h"
#include "window/ui/retained_ui.h"
#include <string.h>

extern fb_info *fb;

#define WITH_PADDING(width, height, p)                                         \
  ((ui_size){.w = width + p + p, .h = height + p + p})

static void layout_simple(ui_node *node, ui_size size, ui_pos pos) {
  node->calculated_size = size;
  node->calculated_pos = pos;
}

extern ui_node *create_node(ui_node *parent);