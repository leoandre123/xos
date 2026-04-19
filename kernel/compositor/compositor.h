#pragma once

#include "compositor_event.h"
#include "fb_info.h"
#include "types.h"

struct task;

#define COMPOSITOR_QUEUE_SIZE 64

int compositor_register(struct task *t);
int compositor_poll(compositor_event *ev);
ulong compositor_window_create(struct task *client, ushort width, ushort height,
                               const char *title, window_handle *handle_out);
void compositor_present_window(window_handle handle);
int compositor_post_event(window_handle handle, window_event *ev);
int compositor_window_poll_event(window_handle handle, window_event *ev);

void compositor_get_framebuffer(window_handle handle, fb_info *fb);