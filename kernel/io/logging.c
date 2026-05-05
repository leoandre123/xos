#include "logging.h"
#include "graphics/console.h"
#include "io/serial.h"
#include "net/networking.h"
#include "net_types.h"
#include "types.h"
#include "utils/formatting.h"
#include <stdarg.h>

void udp_send_old(ipv4_addr dst_addr, ushort src_port, ushort dst_port, void *payload, ushort payload_len);

static bool g_log_to_screen = true;

static bool g_netlog_enabled = false;
static ipv4_addr g_netlog_dst;
static ushort g_netlog_dst_port;
static bool g_netlog_in_send = false;

#define NETLOG_MAX_LINE 480
#define NETLOG_SRC_PORT 1234

void netlog_send(const char *buf, ushort msg_len) {
  if (!g_netlog_enabled || !g_main_driver || g_netlog_in_send)
    return;
  if (msg_len > NETLOG_MAX_LINE)
    msg_len = NETLOG_MAX_LINE;
  g_netlog_in_send = true;
  udp_send_old(g_netlog_dst, NETLOG_SRC_PORT, g_netlog_dst_port, (void *)buf, msg_len);
  // Poll for incoming packets (e.g. ARP reply) so the next send goes through directly
  g_netlog_in_send = false;
}

void logging_enable_network(ipv4_addr dst, ushort dst_port) {
  g_netlog_dst = dst;
  g_netlog_dst_port = dst_port;
  g_netlog_enabled = true;
}

void logging_init() {}

typedef struct {
  log_level level;
  char buf[NETLOG_MAX_LINE];
  ushort len;
} klogf_ctx;

static void klogf_emit(char c, void *ctx) {
  klogf_ctx *kctx = (klogf_ctx *)ctx;
  if (g_log_to_screen)
    console_putc(c);
  serial_write_char(c);
  if (kctx->len < NETLOG_MAX_LINE - 1)
    kctx->buf[kctx->len++] = c;
}

void klogf(log_level level, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  uint old_fg = console_get_fg_color();
  switch (level) {
  case LOG_TRACE:
    console_set_fg_color(0x009e9d99);
    break;
  case LOG_DEBUG:
    console_set_fg_color(0x009e9d99);
    break;
  case LOG_INFO:
    console_set_fg_color(0x00f5f4f0);
    break;
  case LOG_WARNING:
    console_set_fg_color(0x00e0c34f);
    break;
  case LOG_ERROR:
    console_set_fg_color(0x00ff4255);
    break;
  case LOG_CRITICAL:
    console_set_fg_color(0x00ff001a);
    break;
  }

  klogf_ctx ctx = {.level = level, .len = 0};
  emit_formatted_str(klogf_emit, &ctx, fmt, args);

  if (g_log_to_screen)
    console_putc('\n');
  serial_write_char('\n');

  ctx.buf[ctx.len++] = '\n';
  netlog_send(ctx.buf, ctx.len);

  console_set_fg_color(old_fg);
  va_end(args);
}

void logging_set_screen_logging(bool s) {
  g_log_to_screen = s;
}
