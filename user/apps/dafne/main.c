#include "compositor.h"
#include "gfx.h"
#include "keys.h"
#include "syscall.h"
#include "time.h"

#define MAX_ANSI_ARGUMENTS 16

#define IS_ALPHA(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z')

#define BG      0x00C4D6B0
#define TB      0x00381D2A
#define TERM_BG 0x003E363F
#define TERM_FG 0x00DD403A

int main(void) {
  sys_write("Hello from DAFNE (Desktop And File Navigation Environment)!\n");

  sys_write("Mapping fb: ");
  gfx_map(&g_screen);

  char buf[50];
  sprintf(buf, "%x\n", &g_screen);
  sys_write(buf);
  sys_write("creating surface\n");
  g_desktop = gfx_create_surface(g_screen.width, g_screen.height);

  sys_write("inniting compositor\n");
  compositor_init();

  // memset8((ubyte *)&g_windows, 0, sizeof(g_windows));

  sys_write("running navigator\n");
  sys_exec("/sys/programs/navigator.elf");
  sys_exec("/sys/programs/navigator.elf");
  sys_exec("/sys/programs/performance_monitor.elf");
  sys_exec("/sys/programs/terminal_2.elf");
  // sys_read_mouse(&g_mouse);

  sys_write("running loop");
  int target_fps = 125;
  int ms_per_frame = 1000 / target_fps;
  ulong last_frame = sys_unix_time_millis();
  while (1) {
    compositor_update_desktop();
    compositor_handle_events();
    compositor_handle_input();
    compositor_run();

    ulong now = sys_unix_time_millis();
    ulong deadline = last_frame + ms_per_frame;
    if (now < deadline) {
      sleep(deadline - now);
    } else {
      sys_yield();
    }
    last_frame = now;
  }
}
