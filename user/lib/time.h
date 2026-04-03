#pragma once
#include "syscall.h"

typedef struct {
  uint year;
  uint month;
  uint day;
  uint hour;
  uint min;
  uint sec;
} datetime;

static inline int is_leap_year(uint y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static inline datetime time_now(void) {
  ulong ts = sys_time();
  datetime dt;

  dt.sec = ts % 60;
  ts /= 60;
  dt.min = ts % 60;
  ts /= 60;
  dt.hour = ts % 24;
  ts /= 24;

  uint y = 1970;
  while (1) {
    uint days = is_leap_year(y) ? 366 : 365;
    if (ts < days)
      break;
    ts -= days;
    y++;
  }
  dt.year = y;

  uint mdays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (is_leap_year(y))
    mdays[1] = 29;
  uint m = 1;
  for (; m <= 12; m++) {
    if (ts < mdays[m - 1])
      break;
    ts -= mdays[m - 1];
  }
  dt.month = m;
  dt.day = ts + 1;

  return dt;
}
