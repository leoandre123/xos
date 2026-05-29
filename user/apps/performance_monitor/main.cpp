#include "battery_info.h"
#include "cpu_info.h"
#include "gfx.h"
#include "mem_info.h"
#include "process_info.h"
#include "syscall.h"
#include "syscalls.h"
#include "window/ui/retained_ui.h"
#include <stdio.h>
#include <string.h>

#define ROW_COUNT 20

static void format_file_size(char *buf, int size) {
  if (size < 1024) {
    sprintf(buf, "%dB", size);
  } else if (size < 1048576) {
    sprintf(buf, "%dKiB", size / 1024);
  } else {
    sprintf(buf, "%dMiB", size / 1048576);
  }
}

static ui_node *s_grid;
static process_info s_infos[ROW_COUNT];
static int s_info_count = 0;

static int process_list(process_info *buf, int count) {
  return syscall(SYS_PROCESS_LIST, (ulong)buf, count, 0);
}

static void get_mem_info(mem_info *info) {
  syscall(SYS_STATS_MEMORY, (ulong)info, 0, 0);
}

static void get_battery_info(int idx, battery_info *info) {
  syscall(SYS_BATTERY_INFO, idx, (ulong)info, 0);
}

static const char *get_state_string(proc_state state) {
  switch (state) {

  case PROC_READY: return "Ready";
  case PROC_RUNNING: return "Running";
  case PROC_BLOCKED: return "Blocked";
  case PROC_DEAD: return "Dead";
  }
}

static void redraw() {
  ui_mark_dirty(s_grid);
  ui_node *child = s_grid->first_child->next_sibling->next_sibling->next_sibling
                       ->next_sibling;
  char buf[30];
  for (int row = 0; row < ROW_COUNT; row++) {
    bool is_entry = row < s_info_count;
    if (is_entry) {
      sprintf(buf, "%d", s_infos[row].pid);
    }
    ui_label_set_text(child, is_entry ? buf : "");
    child = child->next_sibling;

    ui_label_set_text(child, is_entry ? s_infos[row].name : "");
    child = child->next_sibling;

    ui_label_set_text(child,
                      is_entry ? get_state_string(s_infos[row].state) : "");
    child = child->next_sibling;

    // if (is_entry) {
    //   format_file_size(buf, g_entries[idx].file_size);
    // }
    // ui_label_set_text(child, is_entry ? buf : "");
    child = child->next_sibling;
  }
}

static void update_proc_list() {
  s_info_count = process_list(s_infos, ROW_COUNT);
}

static void on_refresh(ui_node *node) {
  update_proc_list();
  redraw();
}

void create_list_tab(ui_node *tabs) {
  ui_node *list_tab = ui_tab(tabs, "Processes");
  ui_node *vstack = ui_vstack(list_tab, 8, ALIGN_STRETCH);
  ui_node *toolbar = ui_hstack(vstack, 8, ALIGN_CENTER);
  ui_node *refresh = rui_button(toolbar, "Refresh");
  refresh->on_click = on_refresh;

  ui_node *scroll_container = ui_scroll_container(vstack, SCROLL_VERTICAL);
  scroll_container->expand = true;

  s_grid = ui_grid(scroll_container, 4, ROW_COUNT + 1, 4);
  s_grid->bg_color = RGB(232, 230, 228);
  // s_grid->grid.on_row_click = on_dir_clicked;
  s_grid->grid.header_color = RGB(150, 150, 150);
  s_grid->grid.row_alt_color = RGB(250, 240, 240);
  s_grid->grid.row_hover_color = RGB(100, 100, 100);

  ui_grid_set_col_sizing(s_grid, GRID_SIZING_FIT_CONTENT, GRID_SIZING_EXPAND,
                         GRID_SIZING_FIT_CONTENT, GRID_SIZING_FIT_CONTENT);

  ui_label(s_grid, "PID");
  ui_label(s_grid, "Name");
  ui_label(s_grid, "State");
  ui_label(s_grid, "Actions");

  for (int i = 0; i < ROW_COUNT; i++) {
    ui_label(s_grid, "");
    ui_label(s_grid, "");
    ui_label(s_grid, "");
    rui_button(s_grid, "Kill");
  }
}
void create_perf_tab(ui_node *tabs) {
  ui_node *perf_tab = ui_tab(tabs, "Performance");

  ui_node *vstack = ui_vstack(perf_tab, 4, ALIGN_STRETCH);

  mem_info info;
  get_mem_info(&info);

  battery_info bat_info;
  get_battery_info(0, &bat_info);

  char used[10];
  char total[10];
  char buf[40];
  format_file_size(used, info.used);
  format_file_size(total, info.total);

  sprintf(buf, "%s/%s (%d%%)", used, total, (info.used / (info.total / 100)));

  ui_label(vstack, buf);

  cpu_info cpu_info;

  sys_cpu_info(&cpu_info);
  ui_label(vstack, cpu_info.brand);
  sprintf(buf, "CPU Base speed: %dMHz", cpu_info.base_mhz);
  ui_label(vstack, buf);
  sprintf(buf, "CPU Max speed: %dMHz", cpu_info.max_mhz);
  ui_label(vstack, buf);
  sprintf(buf, "Cores: %d", cpu_info.logical_cores);
  ui_label(vstack, buf);

  ui_node *pie = ui_pie_chart(vstack);
  pie_slice slices[] = {{(int)(info.free / 1000), RGB(152, 237, 109)},
                        {(int)(info.used / 1000), RGB(222, 73, 40)}};
  ui_pie_chart_set_slices(pie, slices, 2);

  sprintf(buf, "State: %d", bat_info.state);
  ui_label(vstack, buf);

  sprintf(buf, "Remaining: %d", bat_info.remaining);
  ui_label(vstack, buf);

  sprintf(buf, "Full: %d", bat_info.full_capacity);
  ui_label(vstack, buf);

  sprintf(buf, "Battery: %d%%",
          bat_info.full_capacity
              ? bat_info.remaining / (bat_info.full_capacity / 100)
              : 0);
  ui_label(vstack, buf);
}

int main(void) {

  return 0;
  /*
  window_handle w = window_open(600, 350, "Navigator");

  ui_node *root = ui_create_root();
  root->bg_color = RGB(232, 230, 228);
  ui_node *tabs = ui_tabs(root);

  create_list_tab(tabs);
  create_perf_tab(tabs);

  // s_info_count = process_list(s_infos, ROW_COUNT);
  update_proc_list();
  redraw();

  window_event ev;
  while (window_poll_event(w, &ev)) {
    if (ev.type == WET_CREATE) {
      ui_init(ev.create_event.width, ev.create_event.height,
              ev.create_event.pitch);
    } else if (ev.type == WET_PAINT) {
      if (!ev.paint_event.paint_handle) {
        sys_write("ERROR FB EMPTY");
        for (;;)
          sys_yield();
      }
      ui_render(ev.paint_event.paint_handle);
      window_end_paint(w);
    } else {
      ui_update(ev);
    }
  }

  return 0;
  */
}
