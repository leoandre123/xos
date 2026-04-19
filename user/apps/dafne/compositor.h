#pragma once
#include "gfx.h"
#include "window.h"

#define WINDOW_BORDER_RADIUS 5

extern fb_info g_desktop;
extern fb_info g_screen;
extern fb_info g_backbuffer;
extern window g_windows[WINDOW_MAX_COUNT];

void compositor_init();
void compositor_run();
void compositor_set_cursor(int x, int y);

void compositor_handle_events();