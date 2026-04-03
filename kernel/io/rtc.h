#pragma once
#include "types.h"

typedef struct {
  ubyte second;
  ubyte minute;
  ubyte hour;
  ubyte day;
  ubyte month;
  uint  year;
} rtc_time;

void rtc_read(rtc_time *t);
ulong rtc_unix_timestamp(void);
