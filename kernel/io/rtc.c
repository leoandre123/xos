#include "rtc.h"
#include "io/io.h"

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

static ubyte cmos_read(ubyte reg) {
  outb(CMOS_ADDR, reg);
  return inb(CMOS_DATA);
}

static ubyte bcd_to_bin(ubyte bcd) {
  return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void rtc_read(rtc_time *t) {
  // Wait until no update is in progress
  while (cmos_read(0x0A) & 0x80);

  ubyte second  = cmos_read(0x00);
  ubyte minute  = cmos_read(0x02);
  ubyte hour    = cmos_read(0x04);
  ubyte day     = cmos_read(0x07);
  ubyte month   = cmos_read(0x08);
  ubyte year    = cmos_read(0x09);
  ubyte century = cmos_read(0x32);

  // Check status register B — bit 2 set means binary mode (no BCD conversion needed)
  ubyte status_b = cmos_read(0x0B);
  if (!(status_b & 0x04)) {
    second  = bcd_to_bin(second);
    minute  = bcd_to_bin(minute);
    hour    = bcd_to_bin(hour);
    day     = bcd_to_bin(day);
    month   = bcd_to_bin(month);
    year    = bcd_to_bin(year);
    century = bcd_to_bin(century);
  }

  t->second = second;
  t->minute = minute;
  t->hour   = hour;
  t->day    = day;
  t->month  = month;
  t->year   = (century ? (uint)century * 100 : 2000) + year;
}

static int is_leap(uint y) {
  return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static const int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};

ulong rtc_unix_timestamp(void) {
  rtc_time t;
  rtc_read(&t);

  ulong days = 0;

  // Days in each year from 1970 to t.year-1
  for (uint y = 1970; y < t.year; y++)
    days += is_leap(y) ? 366 : 365;

  // Days in each month of t.year up to t.month-1
  for (uint m = 1; m < t.month; m++) {
    days += days_in_month[m - 1];
    if (m == 2 && is_leap(t.year)) days++;
  }

  days += t.day - 1;

  return days * 86400UL
       + t.hour   * 3600UL
       + t.minute * 60UL
       + t.second;
}
