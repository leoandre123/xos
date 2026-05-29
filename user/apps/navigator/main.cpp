#include "dafne/dafne.h"
#include "dafne/dafne_event.h"
#include "elf_icon.h"
#include "fs/file.h"
#include "gfx.h"
#include "image.h"
#include "stdio.h"
#include "string.h"
#include "syscall.h"
#include "window/ui/retained_ui.h"

#define ROW_COUNT 20
#define MAX_PATH 255
#define MAX_ENTRIES 100

static bitmap *g_folder;
static bitmap *g_file;
static bitmap *g_gear;

static char g_path[MAX_PATH] = "/";
static dirent g_entries[MAX_ENTRIES];
static bitmap *g_entry_icons[MAX_ENTRIES];
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
  while (len > 1 && g_path[len] != '/')
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

static bool is_elf(const char *name) {
  return str_ends_with(name, ".elf") || str_ends_with(name, ".ELF");
}

static bitmap *get_file_icon(int idx) {
  dirent *entry = &g_entries[idx];
  if (entry->is_dir)
    return g_folder;
  if (g_entry_icons[idx])
    return g_entry_icons[idx];
  if (is_elf(entry->name))
    return g_gear;
  return g_file;
}

static void free_entry_icons() {
  for (int i = 0; i < MAX_ENTRIES; i++) {
    if (g_entry_icons[i]) {
      bitmap *bm = g_entry_icons[i];
      ulong sz =
          (ulong)(bm->width * bm->height * sizeof(uint)) + 2 * sizeof(uint);
      sys_free(bm, sz);
      g_entry_icons[i] = 0;
    }
  }
}

static void load_entry_icons() {
  char path[MAX_PATH + 248];
  int plen = strlen(g_path);
  for (int i = 0; i < g_entry_count; i++) {
    if (g_entries[i].is_dir || !is_elf(g_entries[i].name))
      continue;
    int j = 0;
    for (; j < plen; j++)
      path[j] = g_path[j];
    if (plen > 1)
      path[j++] = '/';
    const char *n = g_entries[i].name;
    while (*n)
      path[j++] = *n++;
    path[j] = '\0';
    g_entry_icons[i] = elf_icon_load(path);
  }
}

static void refresh_list() {
  free_entry_icons();
  g_entry_count = file_readdir(g_path, g_entries, MAX_ENTRIES);
  load_entry_icons();
}

static void redraw() {
  ui_mark_dirty(g_grid);
  ui_label_set_text(g_path_lbl, g_path);
  ui_node *child =
      g_grid->first_child->next_sibling->next_sibling->next_sibling;
  for (int row = 0; row < ROW_COUNT; row++) {
    int idx = g_page * ROW_COUNT + row;
    bool is_entry = idx < g_entry_count;

    ui_img_set_img(child, is_entry ? get_file_icon(idx) : 0);
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
  sys_write("[NAVIGATOR] Connecting to dafne...\n");
  if (!dafne_connect()) {
    sys_write("[NAVIGATOR] Connection failed\n");
    return 0;
  }
  sys_write("[NAVIGATOR] Connected\n");
  dafne_window_create(600, 350, "Navigator");

  // window_handle w = window_open(600, 350, "Navigator");

  g_folder = img_load("/sys/icons/folder.lbm");
  g_file = img_load("/sys/icons/file.lbm");
  g_gear = img_load("/sys/icons/gear.lbm");

  ui_node *root = ui_create_root();
  root->bg_color = RGB(232, 230, 228);
  ui_set_perf(true);
  // ui_set_debug_dirty(true);
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

  ui_node *scroll_container = ui_scroll_container(vstack, SCROLL_VERTICAL);
  scroll_container->expand = true;

  g_grid = ui_grid(scroll_container, 3, ROW_COUNT + 1, 4);
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

  window_event ev;
  window_handle w;
  while (dafne_wait_event(&ev)) {
    if (ev.type == WET_CREATE) {
      ui_init(ev.create_event.width, ev.create_event.height,
              ev.create_event.pitch);
      w = ev.create_event.handle;
    } else if (ev.type == WET_PAINT) {
      // sys_write("EVENT");
      if (!ev.paint_event.paint_handle) {
        sys_write("ERROR FB EMPTY");
        for (;;)
          sys_yield();
      }
      ui_render(ev.paint_event.paint_handle);

      dafne_window_present(w);
    } else {
      ui_update(ev);
    }
  }
  sys_write("[NAVIGATOR] Navigator exited\n");
  return 0;
}
