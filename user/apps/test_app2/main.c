#include "fb_info.h"
#include "gfx.h"
#include "image.h"
#include "syscall.h"
#include "window/ui/retained_ui.h"
#include "window/window.h"
#include "window_event.h"
int main(void) {

  window_handle w = window_open(400, 300, "File Navigator");

  bitmap *folder = img_load("/sys/icons/folder.lbm");

  fb_info fb;
  window_get_framebuffer(w, &fb);
  ui_node *root = ui_create_root(&fb);
  root->bg_color = RGB(232, 230, 228);
  ui_node *vstack = ui_vstack(root, 8, ALIGN_STRETCH);
  ui_node *toolbar = ui_hstack(vstack, 8, ALIGN_CENTER);
  ui_node *btn0 = rui_button(toolbar, "btn0");
  ui_node *btn1 = rui_button(toolbar, "Button 1");
  ui_node *btn2 = rui_button(toolbar, "LONG Button 2");
  btn2->padding = 20;

  ui_node *grid = ui_grid(vstack, 3, 5, 4);
  grid->grid.header_color = RGB(150, 150, 150);
  grid->grid.row_alt_color = RGB(250, 200, 200);
  grid->grid.row_hover_color = RGB(100, 100, 100);
  ui_label(grid, "");
  ui_label(grid, "Filename");
  ui_label(grid, "Size");
  ui_img(grid, folder);
  ui_label(grid, "3");
  ui_label(grid, "4");
  ui_img(grid, folder);
  ui_label(grid, "5");
  ui_label(grid, "6");
  ui_img(grid, folder);
  ui_label(grid, "5");
  ui_label(grid, "6");
  ui_img(grid, folder);
  ui_label(grid, "5");
  ui_label(grid, "6");

  window_event ev;
  while (1) {
    while (!window_poll_event(w, &ev)) {
      sys_yield();
    }

    switch (ev.type) {

    case WET_KEY_DOWN:
    case WET_KEY_UP:
    case WET_MOUSE: ui_update(ev); break;
    case WET_RESIZE:
    case WET_MOVE:
    case WET_CLOSE: break;
    }

    ui_render((ui_size){.w = fb.width, .h = fb.height});
    window_end_paint(w);
  }

  return 0;
}
