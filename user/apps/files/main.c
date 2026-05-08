#include "image.h"
#include "mouse.h"
#include "rect.h"
#include "string.h"
#include "syscall.h"
#include "syscalls.h"
#include "window/ui/immediate_ui.h"
#include "window/window.h"
#include "window_event.h"
#include <stdio.h>

#define WIN_W 400
#define WIN_H 480
#define HEADER_H 36
#define ROW_H 22
#define MAX_ENTRIES 128
#define MAX_PATH 512

// mirrors kernel file_dirent (256 bytes)
typedef struct {
  char name[255];
  unsigned char is_dir;
} dirent;

// ── state ────────────────────────────────────────────────────────────────────

static char g_path[MAX_PATH] = "/";
static dirent g_entries[MAX_ENTRIES];
static int g_count = 0;
static int g_scroll = 0;
static int g_selected = -1;

// ── path helpers
// ──────────────────────────────────────────────────────────────

static void path_push(const char *name) {
  sys_write(g_path);
  sys_write(name);
  int plen = strlen(g_path);
  if (plen > 1 && g_path[plen - 1] != '/')
    g_path[plen++] = '/';
  int i = 0;
  while (name[i] && plen + i < MAX_PATH - 1) {
    g_path[plen + i] = name[i];
    i++;
  }
  g_path[plen + i] = '\0';
  sys_write(g_path);
}

static void path_pop(void) {
  int len = strlen(g_path);
  if (len <= 1)
    return;
  if (g_path[len - 1] == '/')
    len--;
  while (len > 1 && g_path[len - 1] != '/')
    len--;
  g_path[len] = '\0';
}

static int name_is_dot(const char *name) {
  // "." or ".." (possibly space-padded by FAT)
  if (name[0] != '.')
    return 0;
  if (name[1] == '\0' || name[1] == ' ')
    return 1;
  if (name[1] == '.' && (name[2] == '\0' || name[2] == ' '))
    return 1;
  return 0;
}

static void refresh(void) {
  sys_write(g_path);
  static dirent tmp[MAX_ENTRIES];
  int n =
      (int)syscall(SYS_FILE_READDIR, (ulong)g_path, (ulong)tmp, MAX_ENTRIES);
  g_count = 0;
  for (int i = 0; i < n; i++) {
    if (!name_is_dot(tmp[i].name))
      g_entries[g_count++] = tmp[i];
  }
  g_scroll = 0;
  g_selected = -1;
}

// ── main
// ──────────────────────────────────────────────────────────────────────

int main(void) {
  refresh();

  window_handle wh = window_open(WIN_W, WIN_H, "Files");
  window_event ev;
  ui_ctx ctx;

  bitmap *folder_icon = img_load("/sys/icons/folder.lbm");

  while (1) {
    while (!window_poll_event(wh, &ev))
      sys_yield();

    switch (ev.type) {
    case WET_MOUSE:
      ctx.mx = (short)ev.mouse_event.x;
      ctx.my = (short)ev.mouse_event.y;
      ctx.clicked = (ev.mouse_event.buttons & MOUSE_BTN_LEFT) != 0;
      break;
    default: break;
    }

    window_begin_paint(wh, &ctx);

    int W = (int)ctx.fb.width;
    int H = (int)ctx.fb.height;

    // ── background
    // ────────────────────────────────────────────────────────────
    ui_rect(&ctx, (rect){0, 0, (ushort)W, (ushort)H}, 0x001E1E2E);

    // ── header bar
    // ────────────────────────────────────────────────────────────
    ui_rect(&ctx, (rect){0, 0, (ushort)W, HEADER_H}, 0x00313244);

    // path label (truncate from left if too long to fit)
    char path_disp[64];
    int plen = strlen(g_path);
    if (plen > 42) {
      path_disp[0] = '.';
      path_disp[1] = '.';
      path_disp[2] = '.';
      int i = 3, j = plen - 39;
      while (g_path[j])
        path_disp[i++] = g_path[j++];
      path_disp[i] = '\0';
    } else {
      int i = 0;
      while (g_path[i]) {
        path_disp[i] = g_path[i];
        i++;
      }
      path_disp[i] = '\0';
    }
    ui_str(&ctx, 8, 10, path_disp, 0x00CDD6F4);

    // "Up" button — disabled visually at root
    int at_root = (g_path[0] == '/' && g_path[1] == '\0');
    if (ui_button(&ctx, W - 52, 6, 46, 24, "Up") && !at_root) {
      path_pop();
      refresh();
    }

    // ── file list
    // ─────────────────────────────────────────────────────────────
    int list_y = HEADER_H;
    int list_h = H - HEADER_H;
    int visible = list_h / ROW_H;
    int end = g_count < g_scroll + visible ? g_count : g_scroll + visible;

    int navigated = 0;
    for (int i = g_scroll; i < end && !navigated; i++) {
      int y = list_y + (i - g_scroll) * ROW_H;
      rect row = {0, (short)y, (ushort)W, ROW_H};

      // row background
      uint bg_col = (i == g_selected) ? 0x00585B70
                    : (i % 2 == 0)    ? 0x001E1E2E
                                      : 0x00242438;
      int hovered = ctx.my >= y && ctx.my < y + ROW_H;
      if (hovered && i != g_selected)
        bg_col = 0x00363654;
      ui_rect(&ctx, row, bg_col);

      // click handling
      if (hovered && ctx.clicked) {
        ctx.clicked = 0;
        if (g_entries[i].is_dir) {
          path_push(g_entries[i].name);
          refresh();
          navigated = 1;
        } else {
          g_selected = i;
        }
      }

      // icon + name
      if (g_entries[i].is_dir) {
        ui_str(&ctx, 8, y + 3, "[+]", 0x0089B4FA);
        // ui_img(&ctx, 8, y + 3, folder_icon);
        ui_str(&ctx, 36, y + 3, g_entries[i].name, 0x0089B4FA);
      } else {
        ui_str(&ctx, 8, y + 3, "   ", 0x00CDD6F4);
        ui_str(&ctx, 36, y + 3, g_entries[i].name, 0x00CDD6F4);
      }
    }

    // ── empty state
    // ───────────────────────────────────────────────────────────
    if (g_count == 0) {
      ui_center_str(&ctx, 0, HEADER_H, W, H - HEADER_H, "Empty", 0x006C7086);
    }

    window_end_paint(wh);
  }

  return 0;
}
