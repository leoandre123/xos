#pragma once

#include "fb_info.h"
#include "types.h"
#include "wm_event.h"

struct task;

#define WM_QUEUE_SIZE 64

int wm_register(struct task *t);
int wm_poll(wm_event *ev);
ulong wm_window_create(struct task *client, ushort width, ushort height,
                       const char *title, window_handle *handle_out);
void wm_present_window(window_handle handle);
int wm_post_event(window_handle handle, window_event *ev);
int wm_window_poll_event(window_handle handle, window_event *ev);

void wm_get_framebuffer(window_handle handle, fb_info *fb);