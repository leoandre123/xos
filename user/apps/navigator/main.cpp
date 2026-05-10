#include "fb_info.h"
#include "fs/file.h"
#include "gfx.h"
#include "image.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"
#include "window/ui/immediate_ui.h"
#include "window/ui/retained_ui.h"
#include "window/window.h"
#include "window_event.h"

#define ROW_COUNT 20
#define MAX_PATH 255

static bitmap *g_folder;
static bitmap *g_file;
static bitmap *g_gear;

static char g_path[MAX_PATH] = "/";
static dirent g_entries[100];
static int g_entry_count = 0;
static int g_page = 0;

static ui_node *g_grid;
static ui_node *g_path_lbl;

static void path_push(const char *name) {
  int plen = strlen(g_path);
  if (plen > 1 && g_path[plen - 1] != '/')
    g_path[plen++] = '/';
  int i = 0;
  while (name[i] && plen + i < MAX_PATH - 1) {
    g_path[plen + i] = name[i];
    i++;
  }
  g_path[plen + i] = '\0';
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

static void format_file_size(char *buf, int size) {
  if (size < 1024) {
    sprintf(buf, "%dB", size);
  } else if (size < 1048576) {
    sprintf(buf, "%dKiB", size / 1024);
  } else {
    sprintf(buf, "%dMiB", size / 1048576);
  }
}

static bitmap *get_file_icon(dirent *entry) {
  if (entry->is_dir) {
    return g_folder;
  } else if (str_ends_with(entry->name, ".ELF")) {
    return g_gear;
  } else {
    return g_file;
  }
}

static void refresh_list() {
  g_entry_count = file_readdir(g_path, g_entries, 100);
}

static void redraw() {
  ui_mark_dirty(g_grid);
  ui_label_set_text(g_path_lbl, g_path);
  ui_node *child =
      g_grid->first_child->next_sibling->next_sibling->next_sibling;
  for (int row = 0; row < ROW_COUNT; row++) {
    int idx = g_page * ROW_COUNT + row;
    bool is_entry = idx < g_entry_count;

    ui_img_set_img(child, is_entry ? get_file_icon(&g_entries[idx]) : 0);
    child = child->next_sibling;

    ui_label_set_text(child, is_entry ? g_entries[idx].name : "");
    child = child->next_sibling;

    char buf[30];
    if (is_entry) {
      format_file_size(buf, g_entries[idx].file_size);
    }
    ui_label_set_text(child, is_entry ? buf : "");
    child = child->next_sibling;
  }
}

void on_dir_clicked(ui_node *grid, int row) {
  if (row == 0)
    return;
  int idx = g_page * ROW_COUNT + row - 1;
  if (idx < g_entry_count && g_entries[idx].is_dir) {
    path_push(g_entries[idx].name);
    refresh_list();
    redraw();
  }
}

void on_up_clicked(ui_node *btn) {
  path_pop();
  refresh_list();
  redraw();
}

int main(void) {

  window_handle w = window_open(600, 500, "Navigator");

  g_folder = img_load("/sys/icons/folder.lbm");
  g_file = img_load("/sys/icons/file.lbm");
  g_gear = img_load("/sys/icons/gear.lbm");

  fb_info fb;
  window_get_framebuffer(w, &fb);
  ui_node *root = ui_create_root(&fb);
  root->bg_color = RGB(232, 230, 228);
  ui_set_perf(true);
  ui_node *vstack = ui_vstack(root, 8, ALIGN_STRETCH);
  ui_node *toolbar = ui_hstack(vstack, 8, ALIGN_CENTER);
  ui_node *btn0 = rui_button(toolbar, "btn0");
  ui_node *btn1 = rui_button(toolbar, "Button 1");
  ui_node *btn2 = rui_button(toolbar, "LONG Button 2");

  ui_node *path_bar = ui_grid(vstack, 3, 1, 8);
  ui_grid_set_col_sizing(path_bar, GRID_SIZING_EXPAND, GRID_SIZING_EXPAND,
                         GRID_SIZING_FIT_CONTENT);
  g_path_lbl = ui_label(path_bar, &g_path[0]);
  g_path_lbl->bg_color = RGB(200, 200, 200);
  ui_text_field(path_bar);
  rui_button(path_bar, "Up")->on_click = on_up_clicked;
  g_grid = ui_grid(vstack, 3, ROW_COUNT + 1, 4);
  g_grid->bg_color = RGB(232, 230, 228);
  g_grid->grid.on_row_click = on_dir_clicked;
  g_grid->grid.header_color = RGB(150, 150, 150);
  g_grid->grid.row_alt_color = RGB(250, 240, 240);
  g_grid->grid.row_hover_color = RGB(100, 100, 100);

  ui_grid_set_col_sizing(g_grid, GRID_SIZING_FIT_CONTENT, GRID_SIZING_EXPAND,
                         GRID_SIZING_FIT_CONTENT);

  ui_label(g_grid, "");
  ui_label(g_grid, "Filename");
  ui_label(g_grid, "Size");
  for (int i = 0; i < ROW_COUNT; i++) {
    ui_img(g_grid, g_file);
    ui_label(g_grid, "");
    ui_label(g_grid, "");
  }

  refresh_list();
  redraw();

  while (1) {
    window_event ev;
    while (window_poll_event(w, &ev)) {
      ui_update(ev);
    }

    ui_render((ui_size){.w = static_cast<ushort>(fb.width),
                        .h = static_cast<ushort>(fb.height)});
    window_end_paint(w);
    sys_yield();
  }

  return 0;
}
