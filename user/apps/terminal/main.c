#include "../../lib/gfx.h"
#include "../../lib/syscall.h"

#define FG 0x00FFFFFF
#define BG 0x00000000

static fb_info g_fb;
static unsigned int g_cols;
static unsigned int g_rows;
static unsigned int g_cx; // cursor column (chars)
static unsigned int g_cy; // cursor row (chars)

static void term_scroll(void) {
  unsigned int pitch_px = g_fb.pitch / 4;
  unsigned int line_h = FONT_GLYPH_HEIGHT;
  // Shift all rows up by one line
  for (unsigned int y = line_h; y < g_fb.height; y++)
    for (unsigned int x = 0; x < g_fb.width; x++)
      g_fb.ptr[(y - line_h) * pitch_px + x] = g_fb.ptr[y * pitch_px + x];
  // Clear the last line
  for (unsigned int y = g_fb.height - line_h; y < g_fb.height; y++)
    for (unsigned int x = 0; x < g_fb.width; x++)
      g_fb.ptr[y * pitch_px + x] = BG;
  if (g_cy > 0)
    g_cy--;
}

static void term_putc(char c) {

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
    gfx_putc(&g_fb, g_cx * FONT_GLYPH_WIDTH, g_cy * FONT_GLYPH_HEIGHT, ' ', FG,
             BG);
    return;
  }
  gfx_putc(&g_fb, g_cx * FONT_GLYPH_WIDTH, g_cy * FONT_GLYPH_HEIGHT, c, FG, BG);
  g_cx++;
  if (g_cx >= g_cols) {
    g_cx = 0;
    g_cy++;
    if (g_cy >= g_rows)
      term_scroll();
  }
}

void _start(void) {
  sys_write("Hello from terminal!\n");
  // Map framebuffer and clear screen
  gfx_map(&g_fb);
  g_cols = g_fb.width / FONT_GLYPH_WIDTH;
  g_rows = g_fb.height / FONT_GLYPH_HEIGHT;
  g_cx = 0;
  g_cy = 0;
  gfx_fill(&g_fb, BG);

  // shell->terminal: terminal reads rendered text, shell writes output
  ulong p1 = sys_pipe();
  int shell_out_r = (int)(p1 & 0xFFFFFFFF);
  int shell_out_w = (int)(p1 >> 32);

  // terminal->shell: terminal writes keyboard chars, shell reads input
  ulong p2 = sys_pipe();
  int shell_in_r = (int)(p2 & 0xFFFFFFFF);
  int shell_in_w = (int)(p2 >> 32);

  // Spawn shell with its stdin=shell_in_r, stdout=shell_out_w
  sys_exec_fds("/shell.elf", shell_in_r, shell_out_w);

  // Main loop: render shell output, forward keyboard input
  while (1) {
    // Drain any pending output from shell
    int avail = sys_pipe_avail(shell_out_r);
    if (avail > 0) {
      char buf[64];
      int n = (avail < 64) ? avail : 64;
      n = sys_read_fd(shell_out_r, buf, (ulong)n);
      for (int i = 0; i < n; i++)
        term_putc(buf[i]);
    }

    // Forward keyboard input to shell (non-blocking)
    ulong key = sys_read_key_nb();
    char c = (char)(key >> 32);
    if (c)
      sys_write_fd(shell_in_w, &c, 1);

    sys_yield();
  }
}
