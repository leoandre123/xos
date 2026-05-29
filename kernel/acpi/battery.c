#include "battery.h"
#include "acpi/acpi.h"
#include "acpi/aml.h"
#include "acpi/ec.h"
#include "io/logging.h"
#include "memory/vmm.h"

void battery_init(void) {
  ec_init();

  ulong dsdt_phys = acpi_get_dsdt_phys();
  if (!dsdt_phys) {
    klogf(LOG_WARNING, "Battery: no DSDT, battery info unavailable");
    return;
  }

  // Scan DSDT
  acpi_header *dsdt = (acpi_header *)PHYS_TO_HHDM(dsdt_phys);
  ubyte *aml        = (ubyte *)(dsdt + 1);
  uint   aml_len    = dsdt->length - (uint)sizeof(acpi_header);
  aml_scan(aml, aml_len);

  // Scan all SSDTs (QEMU -acpitable injects battery devices as SSDTs)
  for (uint i = 0; ; i++) {
    acpi_header *ssdt = acpi_find_table_nth("SSDT", i);
    if (!ssdt) break;
    ubyte *ssdt_aml = (ubyte *)(ssdt + 1);
    uint   ssdt_len = ssdt->length - (uint)sizeof(acpi_header);
    klogf(LOG_DEBUG, "Battery: scanning SSDT[%u] oem=%.6s", i, ssdt->oem_id);
    aml_scan(ssdt_aml, ssdt_len);
  }

  klogf(LOG_INFO, "Battery: %u battery device(s) found", aml_battery_count());
}

uint battery_count(void) {
  return aml_battery_count();
}

bool battery_get(uint idx, battery_info *out) {
  if (idx >= aml_battery_count()) return false;

  aml_bst bst;
  if (!aml_eval_bst(idx, &bst)) return false;

  out->present    = true;
  out->state      = bst.state;
  out->rate       = bst.rate;
  out->remaining  = bst.remaining;
  out->voltage_mv = bst.voltage;

  // Try to get design/full capacity from _BIF or _BIX
  aml_bif bif;
  if (aml_eval_bif(idx, &bif)) {
    out->full_capacity = bif.full_capacity;
    out->power_unit    = bif.power_unit;
  } else {
    out->full_capacity = 0;
    out->power_unit    = 0;
  }
  return true;
}
