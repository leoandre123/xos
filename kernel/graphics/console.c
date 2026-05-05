#include "console.h"
#include "font.h"
#include "memory/memutils.h"
#include "types.h"
#include "utils/formatting.h"
#include <stdarg.h>

static volatile uint *fb;
static uint fb_width;
static uint fb_height;
static uint fb_pitch_pixels;

static uint cols;
static uint rows;

static uint cursor_x;
static uint cursor_y;

static uint fg = 0x00ffffff;
static uint bg = 0x00000000;

void console_set_fg_color(uint value) { fg = value; }
void console_set_bg_color(uint value) { bg = value; }
uint console_get_fg_color() { return fg; }
uint console_get_bg_color() { return bg; }

// Shadow buffer: avoids slow reads from framebuffer during scroll.
// Scroll shifts this RAM buffer (fast), then re-renders as sequential writes only.
#define SHADOW_MAX_COLS 256
#define SHADOW_MAX_ROWS 96

typedef struct {
  char ch;
  uint fg;
  uint bg;
} cell_t;

static cell_t shadow[SHADOW_MAX_ROWS][SHADOW_MAX_COLS];

static void render_cell(uint col, uint row) {
  cell_t *c = &shadow[row][col];
  ubyte idx = (ubyte)(c->ch - 32);
  const ubyte *glyph = kernel_font[idx < FONT_GLYPH_COUNT ? idx : 31];
  uint base_x = col * FONT_GLYPH_WIDTH;
  uint base_y = row * FONT_GLYPH_HEIGHT;
  uint fg_color = c->fg;
  uint bg_color = c->bg;

  for (uint py = 0; py < FONT_GLYPH_HEIGHT; py++) {
    uint *row_ptr = (uint *)fb + (base_y + py) * fb_pitch_pixels + base_x;
    ubyte bits = glyph[py];
    for (uint px = 0; px < FONT_GLYPH_WIDTH; px++)
      row_ptr[px] = (bits >> (7 - px)) & 1 ? fg_color : bg_color;
  }
}

static void draw_char_at(uint col, uint row, char c) {
  if (col >= SHADOW_MAX_COLS || row >= SHADOW_MAX_ROWS)
    return;
  shadow[row][col] = (cell_t){c, fg, bg};
  render_cell(col, row);
}

static void scroll(int n_rows) {
  uint keep = rows - n_rows;

  // Shift shadow rows in RAM — no framebuffer reads
  memcpy8((ubyte *)shadow, (ubyte *)(shadow + n_rows), keep * sizeof(shadow[0]));

  // Clear vacated rows in shadow
  cell_t blank = {' ', fg, bg};
  for (uint r = keep; r < rows; r++)
    for (uint c = 0; c < cols; c++)
      shadow[r][c] = blank;

  cursor_y -= n_rows;

  // Re-render full screen: sequential writes only, no framebuffer reads
  for (uint r = 0; r < rows; r++)
    for (uint c = 0; c < cols; c++)
      render_cell(c, r);
}

void console_init(ulong fb_base, uint width, uint height, uint pitch) {
  fb = (volatile uint *)fb_base;
  fb_width = width;
  fb_height = height;
  fb_pitch_pixels = pitch / 4;

  cols = width / FONT_GLYPH_WIDTH;
  rows = height / FONT_GLYPH_HEIGHT;

  if (cols > SHADOW_MAX_COLS)
    cols = SHADOW_MAX_COLS;
  if (rows > SHADOW_MAX_ROWS)
    rows = SHADOW_MAX_ROWS;

  cursor_x = 0;
  cursor_y = 0;
}

void console_clear(uint color) {
  bg = color;
  cell_t blank = {' ', fg, bg};
  for (uint r = 0; r < rows; r++)
    for (uint c = 0; c < cols; c++)
      shadow[r][c] = blank;

  for (uint y = 0; y < fb_height; y++)
    for (uint x = 0; x < fb_width; x++)
      fb[y * fb_pitch_pixels + x] = color;

  cursor_x = 0;
  cursor_y = 0;
}

void console_putc(char c) {
  if (c == '\n') {
    cursor_x = 0;
    cursor_y++;
    if (cursor_y >= rows)
      scroll(cursor_y - rows + 1);
    return;
  }

  draw_char_at(cursor_x, cursor_y, c);
  cursor_x++;

  if (cursor_x >= cols) {
    cursor_x = 0;
    cursor_y++;
  }
}

void console_write(const char *s) {
  while (*s)
    console_putc(*s++);
}

void console_write_line(const char *s) {
  console_write(s);
  console_putc('\n');
}

void console_write_hex64(ulong value) {
  static const char *hex = "0123456789ABCDEF";
  console_write("0x");
  for (int i = 15; i >= 0; i--) {
    ubyte nibble = (value >> (i * 4)) & 0xF;
    console_putc(hex[nibble]);
  }
}

void console_write_u32(uint value) {
  char buf[16];
  int i = 0;

  if (value == 0) {
    console_putc('0');
    return;
  }

  while (value > 0) {
    buf[i++] = '0' + (value % 10);
    value /= 10;
  }

  while (i--)
    console_putc(buf[i]);
}

void console_set_cursor(int x, int y) {
  cursor_x = x;
  cursor_y = y;
}

void console_emit_char(char ch, void *ctx) {
  (void)ctx;
  console_putc(ch);
}

void console_writef(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emit_formatted_str(console_emit_char, 0, fmt, args);
  va_end(args);
}
