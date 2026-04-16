#include "font.h"
#include "gfx.h"
#include "syscall.h"
#include "time.h"
#include <stdio.h>

#define MAX_ANSI_ARGUMENTS 16

#define IS_ALPHA(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z')

#define BG 0x00C4D6B0
#define TB 0x00381D2A
#define TERM_BG 0x003E363F
#define TERM_FG 0x00DD403A

static fb_info g_fb;

static int g_shell_out_r;
static int g_shell_out_w;
static int g_shell_in_r;
static int g_shell_in_w;

static uint g_term_x;
static uint g_term_y;
static uint g_term_width;
static uint g_term_height;
static uint g_cols;
static uint g_rows;
static uint g_cx;
static uint g_cy;

static uint g_fg;
static uint g_bg;

static ulong g_time;

static uint g_colors[] = {
    0x001E1E2E, // Black   (dark base)
    0x00F38BA8, // Red     (soft rose)
    0x00A6E3A1, // Green   (mint)
    0x00F9E2AF, // Yellow  (warm sand)
    0x0089B4FA, // Blue    (sky)
    0x00CBA6F7, // Magenta (lavender)
    0x0094E2D5, // Cyan    (teal mist)
    0x00CDD6F4, // White   (soft white)
};

static void term_scroll(void) {
  unsigned int pitch_px = g_fb.pitch / 4;
  unsigned int line_h = FONT_GLYPH_HEIGHT;
  // Shift all rows up by one line
  for (unsigned int y = line_h; y < g_term_height; y++)
    for (unsigned int x = 0; x < g_term_width; x++)
      g_fb.ptr[(g_term_y + y - line_h) * pitch_px + g_term_x + x] =
          g_fb.ptr[g_term_y + y * pitch_px + g_term_x + x];
  // Clear the last line
  for (unsigned int y = g_fb.height - line_h; y < g_fb.height; y++)
    for (unsigned int x = 0; x < g_fb.width; x++)
      g_fb.ptr[g_term_y + y * pitch_px + g_term_x + x] = g_bg;
  if (g_cy > 0)
    g_cy--;
}

static void reset_style() {
  g_fg = g_colors[7];
  g_bg = g_colors[0];
}

static void parse_ansi(char *code) {

  //\e[31;54m

  int args[MAX_ANSI_ARGUMENTS] = {0};
  int argc = 0;

  if (*code++ != '\e')
    return;
  if (*code++ != '[')
    return;

  while (!IS_ALPHA(*code)) {
    if (*code == ';') {
      if (argc == MAX_ANSI_ARGUMENTS - 1) {
        return;
      }
      argc++;
    } else if (*code >= '0' && *code <= '9') {
      args[argc] = args[argc] * 10 + (*code - '0');
    }
    code++;
  }

  argc++;

  switch (*code) {
  case 'H': {
    g_cx = 0;
    g_cy = 0;
    break;
  }
  case 'm': {
    for (int i = 0; i < argc; i++) {
      if (args[i] == 0) {
        reset_style();
      } else if (args[i] >= 30 && args[i] <= 37) {
        g_fg = g_colors[args[i] - 30];
      } else if (args[i] >= 40 && args[i] <= 47) {
        g_bg = g_colors[args[i] - 40];
      }
    }
  }
  }
}

static void term_putc(char c) {
  static int is_espace = 0;
  static char ansi_sequence[32];
  static int ansi_sequence_index = 0;
  if (c == '\e') {
    is_espace = 1;
    ansi_sequence_index = 0;
  }
  if (is_espace) {
    ansi_sequence[ansi_sequence_index] = c;
    ansi_sequence_index++;
    if (IS_ALPHA(c)) {
      parse_ansi(ansi_sequence);
      is_espace = 0;
    }
    return;
  }
  if (c == '\n') {
    g_cx = 0;
    g_cy++;
    if (g_cy >= g_rows)
      term_scroll();
    return;
  }
  if (c == '\b') {
    if (g_cx > 0)
      g_cx--;
    gfx_putc(&g_fb, g_term_x + g_cx * FONT_GLYPH_WIDTH,
             g_term_y + g_cy * FONT_GLYPH_HEIGHT, ' ', g_fg, g_bg);
    return;
  }
  gfx_putc(&g_fb, g_term_x + g_cx * FONT_GLYPH_WIDTH,
           g_term_y + g_cy * FONT_GLYPH_HEIGHT, c, g_fg, g_bg);
  g_cx++;
  if (g_cx >= g_cols) {
    g_cx = 0;
    g_cy++;
    if (g_cy >= g_rows)
      term_scroll();
  }
}

static void init_term() {

  g_term_width = FONT_GLYPH_WIDTH * 40;
  g_term_height = g_fb.height - 50;
  g_term_x = g_fb.width - g_term_width;
  g_term_y = 0;

  g_cols = g_term_width / FONT_GLYPH_WIDTH;
  g_rows = g_term_height / FONT_GLYPH_HEIGHT;
  g_cx = 0;
  g_cy = 0;

  // shell->terminal: terminal reads rendered text, shell writes output
  ulong p1 = sys_pipe();
  g_shell_out_r = (int)(p1 & 0xFFFFFFFF);
  g_shell_out_w = (int)(p1 >> 32);

  // terminal->shell: terminal writes keyboard chars, shell reads input
  ulong p2 = sys_pipe();
  g_shell_in_r = (int)(p2 & 0xFFFFFFFF);
  g_shell_in_w = (int)(p2 >> 32);

  // Spawn shell with its stdin=shell_in_r, stdout=shell_out_w
  sys_exec_fds("/shell.elf", g_shell_in_r, g_shell_out_w);

  gfx_rect(&g_fb, g_term_x, g_term_y, g_term_width, g_term_height, TERM_BG);
}
static void update_term() {
  // Drain any pending output from shell
  int avail = sys_pipe_avail(g_shell_out_r);
  if (avail > 0) {
    char buf[64];
    int n = (avail < 64) ? avail : 64;
    n = sys_read_fd(g_shell_out_r, buf, (ulong)n);
    for (int i = 0; i < n; i++)
      term_putc(buf[i]);
  }

  // Forward keyboard input to shell (non-blocking)
  ulong key = sys_read_key_nb();
  char c = (char)(key >> 32);
  if (c)
    sys_write_fd(g_shell_in_w, &c, 1);
}

int main(void) {
  sys_write("Hello from DAFNE (Desktop And File Navigation Environment)!\n");
  // Map framebuffer and clear screen
  gfx_map(&g_fb);
  gfx_fill(&g_fb, BG);
  gfx_rect(&g_fb, 0, g_fb.height - 50, g_fb.width, 50, TB);

  char time[20];

  gfx_str(&g_fb, g_fb.width - 180, g_fb.height - 32, time, 0x00A22F5C, TB);

  init_term();

  while (1) {
    ulong ts = sys_time();
    if (ts != g_time) {
      g_time = ts;
      datetime dt = to_datetime(ts);
      sprintf(time, "%02u:%02u:%02u\n", dt.hour, dt.min, dt.sec);
      gfx_str(&g_fb, g_fb.width - FONT_GLYPH_WIDTH * 10, g_fb.height - 40, time,
              0x00A22F5C, TB);
      sprintf(time, "%04u-%02u-%02u", dt.year, dt.month, dt.day);
      gfx_str(&g_fb, g_fb.width - FONT_GLYPH_WIDTH * 12, g_fb.height - 20, time,
              0x00A22F5C, TB);
    }
    update_term();
    sys_yield();
  }
}
