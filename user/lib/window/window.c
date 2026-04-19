#include "window.h"
#include "fb_info.h"
#include "syscall.h"
#include "syscalls.h"

window_handle window_open(ushort width, ushort height, const char *title) {
  window_create_options opts = {width, height, title};
  return syscall(SYS_WINDOW_CREATE, (ulong)&opts, 0, 0);
}

void window_present(window_handle wh) { syscall(SYS_WINDOW_PRESENT, wh, 0, 0); }

int window_poll_event(window_handle wh, window_event *ev) {
  return (int)syscall(SYS_WINDOW_POLL, wh, (ulong)ev, 0);
}

void window_get_framebuffer(window_handle wh, fb_info *fb) {
  syscall(SYS_WINDOW_FRAMEBUFFER, wh, (ulong)fb, 0);
}

void window_begin_paint(window_handle wh, ui_ctx *ctx) {
  syscall(SYS_WINDOW_FRAMEBUFFER, wh, (ulong)&ctx->fb, 0);
  if (ctx->fb.ptr) {
    ctx->ui_rect.x = 0;
    ctx->ui_rect.y = 0;
    ctx->ui_rect.w = ctx->fb.width;
    ctx->ui_rect.h = ctx->fb.height;
  }
}
void window_end_paint(window_handle wh) { window_present(wh); }