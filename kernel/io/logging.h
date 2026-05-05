#pragma once

#include "net_types.h"
#include "types.h"

typedef enum {
  LOG_TRACE,
  LOG_DEBUG,
  LOG_INFO,
  LOG_WARNING,
  LOG_ERROR,
  LOG_CRITICAL
} log_level;

typedef enum {
  screen,
  serial,
  network,
} logging_mode;

void logging_init();
void klogf(log_level level, const char *fmt, ...);

void logging_set_screen_logging(bool s);
void logging_enable_network(ipv4_addr dst, ushort dst_port);