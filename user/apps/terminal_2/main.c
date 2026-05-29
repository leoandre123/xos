#include "gfx.h"
#include "keyboard.h"
#include "syscall.h"

#define MAX_ANSI_ARGUMENTS 16

#define IS_ALPHA(c) (c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z')

static fb_info g_fb; // drawing surface — always hardware buffer 0
static unsigned int g_cols;
static unsigned int g_rows;
static unsigned int g_cx;
static unsigned int g_cy;

static uint g_fg;
static uint g_bg;

static uint g_colors[] = {
    0xFF1E1E2E, // Black   (dark base)
    0xFFF38BA8, // Red     (soft rose)
    0xFFA6E3A1, // Green   (mint)
    0xFFF9E2AF, // Yellow  (warm sand)
    0xFF89B4FA, // Blue    (sky)
    0xFFCBA6F7, // Magenta (lavender)
    0xFF94E2D5, // Cyan    (teal mist)
    0xFFCDD6F4, // White   (soft white)
};

static void term_scroll(void) {
  unsigned int pitch_px = g_fb.pitch / 4;
  unsigned int line_h = FONT_GLYPH_HEIGHT;
  for (unsigned int y = line_h; y < g_fb.height; y++)
    for (unsigned int x = 0; x < g_fb.width; x++)
      g_fb.ptr[(y - line_h) * pitch_px + x] = g_fb.ptr[y * pitch_px + x];
  for (unsigned int y = g_fb.height - line_h; y < g_fb.height; y++)
    for (unsigned int x = 0; x < g_fb.width; x++)
      g_fb.ptr[y * pitch_px + x] = g_bg;
  if (g_cy > 0)
    g_cy--;
}

static void reset_style() {
  g_fg = g_colors[7];
  g_bg = g_colors[0];
}

static void parse_ansi(char *code) {
  int args[MAX_ANSI_ARGUMENTS] = {0};
  int argc = 0;

  if (*code++ != '\e')
    return;
  if (*code++ != '[')
    return;

  while (!IS_ALPHA(*code)) {
    if (*code == ';') {
      if (argc == MAX_ANSI_ARGUMENTS - 1)
        return;
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
    gfx_putc(&g_fb, g_cx * FONT_GLYPH_WIDTH, g_cy * FONT_GLYPH_HEIGHT, ' ',
             g_fg);
    return;
  }
  gfx_putc(&g_fb, g_cx * FONT_GLYPH_WIDTH, g_cy * FONT_GLYPH_HEIGHT, c, g_fg);
  g_cx++;
  if (g_cx >= g_cols) {
    g_cx = 0;
    g_cy++;
    if (g_cy >= g_rows)
      term_scroll();
  }
}
/*
// Copy g_fb content to hw_ptr if it differs, then present.
static void do_present(window_handle wh, uint *hw_ptr) {
  if (hw_ptr != g_fb.ptr) {
    uint n = g_fb.width * g_fb.height;
    for (uint i = 0; i < n; i++)
      hw_ptr[i] = g_fb.ptr[i];
  }
  window_present(wh);
}
  */

int main(void) {

  return 0;
  /*
  window_handle wh = window_open(500, 400, "Terminal");

  reset_style();

  window_get_framebuffer(wh, &g_fb);
  gfx_clear_clip(&g_fb);
  g_cols = g_fb.width / FONT_GLYPH_WIDTH;
  g_rows = g_fb.height / FONT_GLYPH_HEIGHT;
  g_cx = 0;
  g_cy = 0;
  gfx_fill(&g_fb, g_bg);
  term_putc('t');

  // Prime both hardware buffers so neither starts black.
  do_present(wh, g_fb.ptr);
  do_present(wh, g_fb.ptr);

  // shell->terminal pipe
  ulong p1 = sys_pipe();
  int shell_out_r = (int)(p1 & 0xFFFFFFFF);
  int shell_out_w = (int)(p1 >> 32);

  // terminal->shell pipe
  ulong p2 = sys_pipe();
  int shell_in_r = (int)(p2 & 0xFFFFFFFF);
  int shell_in_w = (int)(p2 >> 32);

  sys_exec_fds("/sys/programs/shell.elf", shell_in_r, shell_out_w);

  window_event ev;
  while (1) {
    if (window_poll_event(wh, &ev)) {
      switch (ev.type) {
      case WET_KEY_DOWN: {
        if (ev.key_event.character)
          sys_write_fd(shell_in_w, &ev.key_event.character, 1);
      } break;
      case WET_KEY_UP:
      case WET_MOUSE:
      case WET_RESIZE:
      case WET_MOVE:
      case WET_CLOSE: break;
      case WET_PAINT:
        do_present(wh, (uint *)ev.paint_event.paint_handle);
        break;
      }
    }

    int avail = sys_pipe_avail(shell_out_r);
    if (avail > 0) {
      char buf[64];
      int n = (avail < 64) ? avail : 64;
      n = sys_read_fd(shell_out_r, buf, (ulong)n);
      for (int i = 0; i < n; i++)
        term_putc(buf[i]);
      do_present(wh, g_fb.ptr);
    }
  }
    */
}
