
#include "font.h"
#include "gfx.h"
#include "keyboard.h"
#include "keys.h"
#include "types.h"
#include "window/ui/retained_ui.h"
#include "window/ui/retained_ui_internal.h"
#include <string.h>

static ui_size ps_text_field(ui_node *node) {
  ushort w = strlen(node->label.text) * FONT_GLYPH_WIDTH;
  ushort h = FONT_GLYPH_HEIGHT;
  return WITH_PADDING(w, h, node->padding);
}

static void draw_text_field(ui_node *node) {
  if (node->focused) {
    gfx_rect_outline(&g_ui_current_fb, node->calculated_pos.x,
                     node->calculated_pos.y, node->calculated_size.w,
                     node->calculated_size.h, RGB(0, 0, 0));
    int len = strlen(node->text_field.text);
    if (g_ui_current_time % 2) {
      int x = node->calculated_pos.x + len * FONT_GLYPH_WIDTH + 2;
      gfx_line(&g_ui_current_fb, x, node->calculated_pos.y, x,
               node->calculated_pos.y + node->calculated_size.h,
               RGB(50, 50, 50));
    }
  }

  gfx_str(&g_ui_current_fb, node->calculated_pos.x + node->padding,
          node->calculated_pos.y + node->padding, node->text_field.text,
          RGB(0, 0, 0));
}

static void text_field_on_key(ui_node *node, KeyEvent ev) {
  int len = strlen(node->text_field.text) + 1;

  if (ev.code == KEY_BACKSPACE) {
    if (len > 1) {
      node->text_field.text[len - 2] = '\0';
      ui_mark_dirty(node);
    }

    return;
  }

  if (len >= TEXT_FIELD_MAX_LENGTH) {
    return;
  }
  node->text_field.text[len - 1] = ev.character;
  node->text_field.text[len] = '\0';
  ui_mark_dirty(node);
}

static void text_field_update(ui_node *node) {
  static ulong last_time = 0;
  if (node->focused && g_ui_current_time != last_time) {
    last_time = g_ui_current_time;
    ui_mark_dirty(node);
  }
}

ui_node *ui_text_field(ui_node *parent) {
  ui_node *node = MAKE_NODE(parent, UI_LABEL, ps_text_field, layout_simple,
                            draw_text_field);
  node->update = text_field_update;
  node->_on_key = text_field_on_key;
  node->interactive = true;
  node->text_field.text[0] = '\0';
  return node;
}