#pragma once
#include "types.h"

// Filled by SYS_BATTERY_INFO.  All capacity/rate values are in mWh/mW
// when power_unit==0, or mAh/mA when power_unit==1.
typedef struct {
  bool present;
  uint state;         // 0=full, 1=discharging, 2=charging, 4=critical
  uint rate;          // present rate (mW or mA)
  uint remaining;     // remaining capacity (mWh or mAh)
  uint voltage_mv;    // present voltage in mV
  uint full_capacity; // last full charge capacity (mWh or mAh)
  uint power_unit;    // 0=mW/mWh, 1=mA/mAh
} battery_info;
