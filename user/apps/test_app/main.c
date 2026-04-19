#include "mouse.h"
#include "rect.h"
#include "syscall.h"
#include "window/ui/immediate_ui.h"
#include "window/window.h"
#include "window_event.h"
#include <stdio.h>
int main(void) {

  window_handle w = window_open(400, 300, "User window");

  ui_ctx ctx; // = {.fb = &w.fb, .mx = 0, .my = 0, .clicked = false};

  bool show_menu = false;

  int x = 0;

  window_event ev;
  while (1) {
    while (!window_poll_event(w, &ev)) {
      sys_yield();
    }

    switch (ev.type) {
    case WET_KEY_DOWN: {
    } break;
    case WET_KEY_UP:
    case WET_MOUSE:
      ctx.mx = ev.mouse_event.x;
      ctx.my = ev.mouse_event.y;
      ctx.clicked = ev.mouse_event.buttons & MOUSE_BTN_LEFT;
      break;
    case WET_RESIZE:
    case WET_MOVE:
      break;
    case WET_CLOSE:
      break;
    }

    window_begin_paint(w, &ctx);

    ui_rect(&ctx, ctx.ui_rect, 0x00102040);
    if (ui_button(&ctx, 50, 50, 100, 25, "BUTTON")) {
      x++;
      sys_exec("/terminal_2.elf");
    }

    rect rc = {
        50,
        150,
        100,
        50,
    };

    ui_rounded_rect(&ctx, rc, 25, 0x00401010);
    char buf[5];
    sprintf(buf, "%d", x);
    ui_center_str(&ctx, rc.x, rc.y, rc.w, rc.h, buf, 0x00ff00ff);

    ui_checkbox(&ctx, 50, 80, "Show menu", &show_menu);
    if (show_menu) {
      ui_button(&ctx, 75, 100, 50, 25, "Hej");
    }

    window_end_paint(w);
  }

  return 0;
}
