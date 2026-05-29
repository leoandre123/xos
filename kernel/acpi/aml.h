#pragma once
#include "types.h"

// Battery status returned by the ACPI _BST method (Package of 4 integers).
typedef struct {
  uint state;     // bit 0=discharging, bit 1=charging, bit 2=critical
  uint rate;      // present rate in mW (or mA when power_unit=1)
  uint remaining; // remaining capacity in mWh (or mAh)
  uint voltage;   // present voltage in mV
} aml_bst;

// Static battery info from the ACPI _BIF/_BIX method.
typedef struct {
  uint power_unit;       // 0=mW/mWh, 1=mA/mAh
  uint design_capacity;  // design capacity in mWh or mAh
  uint full_capacity;    // last full charge capacity
  uint design_voltage;   // design voltage in mV
} aml_bif;

// Scan all AML in a table body (DSDT or SSDT).
// Must be called once per table before querying battery info.
void aml_scan(const ubyte *aml, uint len);

uint aml_battery_count(void);
bool aml_eval_bst(uint idx, aml_bst *out);
bool aml_eval_bif(uint idx, aml_bif *out);
