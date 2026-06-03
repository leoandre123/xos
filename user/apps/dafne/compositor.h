#pragma once
#include "window.h"

#define STATUS_BAR_HEIGHT      20
#define STATUS_BAR_TEXT_HEIGHT FONT_GLYPH_HEIGHT

#define APP_BAR_WIDTH       70
#define APP_BAR_ICON_SIZE   40
#define APP_BAR_ICON_GAP    10
#define APP_BAR_ICON_RADIUS 10
#define APP_BAR_ICON_MARGIN ((APP_BAR_WIDTH - APP_BAR_ICON_SIZE) / 2)

extern fb_info g_desktop;
extern fb_info g_screen;
extern fb_info g_backbuffer;
// extern window g_windows[WINDOW_MAX_COUNT];

void compositor_init();
void compositor_run();
void compositor_set_cursor(int x, int y);
void compositor_handle_events();
void compositor_handle_input();
void compositor_update_desktop();

void window_focus(window *w);
void window_focus_next();
void window_move_to(window *w, int x, int y);
void window_show(window *w, bool show);
void window_swap_buffers(window *w);
