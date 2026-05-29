#include "acpi.h"
#include "io/logging.h"
#include "memory/heap.h"
#include "memory/vmm.h"
#include <stddef.h>

static acpi_header **g_tables;
static uint g_table_count;
static ulong g_dsdt_phys;

static bool sig_eq(const char *a, const char *b4) {
  return a[0] == b4[0] && a[1] == b4[1] && a[2] == b4[2] && a[3] == b4[3];
}

void acpi_init(ulong rsdp_phys) {
  if (!rsdp_phys) {
    klogf(LOG_WARNING, "ACPI: no RSDP — ACPI unavailable");
    return;
  }

  acpi_rsdp *rsdp = (acpi_rsdp *)PHYS_TO_HHDM(rsdp_phys);

  // Validate signature
  const char *expected = "RSD PTR ";
  for (int i = 0; i < 8; i++) {
    if (rsdp->sig[i] != expected[i]) {
      klogf(LOG_ERROR, "ACPI: bad RSDP signature");
      return;
    }
  }
  klogf(LOG_INFO, "ACPI: RSDP rev=%u at 0x%lx", rsdp->revision, rsdp_phys);

  // Use XSDT (64-bit entries) when available, RSDT (32-bit) otherwise
  if (rsdp->revision >= 2 && rsdp->xsdt_phys) {
    acpi_header *xsdt = (acpi_header *)PHYS_TO_HHDM(rsdp->xsdt_phys);
    uint n = (xsdt->length - sizeof(acpi_header)) / 8;
    ulong *entries = (ulong *)(xsdt + 1);
    g_tables = kmalloc(n * sizeof(acpi_header *));
    g_table_count = n;
    for (uint i = 0; i < n; i++) {
      g_tables[i] = (acpi_header *)PHYS_TO_HHDM(entries[i]);
      klogf(LOG_DEBUG, "ACPI:  %.4s phys=0x%lx", g_tables[i]->sig, entries[i]);
    }
  } else {
    acpi_header *rsdt = (acpi_header *)PHYS_TO_HHDM((ulong)rsdp->rsdt_phys);
    uint n = (rsdt->length - sizeof(acpi_header)) / 4;
    uint *entries = (uint *)(rsdt + 1);
    g_tables = kmalloc(n * sizeof(acpi_header *));
    g_table_count = n;
    for (uint i = 0; i < n; i++) {
      g_tables[i] = (acpi_header *)PHYS_TO_HHDM((ulong)entries[i]);
      klogf(LOG_DEBUG, "ACPI:  %.4s phys=0x%x", g_tables[i]->sig, entries[i]);
    }
  }

  // Locate DSDT via the FADT ("FACP")
  acpi_header *fadt = acpi_find_table("FACP");
  if (fadt) {
    ubyte *f = (ubyte *)fadt;
    // ACPI 2.0+ X_DSDT at byte offset 140 (8 bytes)
    ulong xdsdt = 0;
    if (fadt->revision >= 2 && fadt->length > 148)
      xdsdt = *(ulong *)(f + 140);
    g_dsdt_phys = xdsdt ? xdsdt : (ulong)(*(uint *)(f + 40));
    klogf(LOG_INFO, "ACPI: DSDT phys=0x%lx", g_dsdt_phys);
  } else {
    klogf(LOG_WARNING, "ACPI: FADT not found, no DSDT");
  }
}

acpi_header *acpi_find_table(const char *sig4) {
  return acpi_find_table_nth(sig4, 0);
}

acpi_header *acpi_find_table_nth(const char *sig4, uint n) {
  uint found = 0;
  for (uint i = 0; i < g_table_count; i++) {
    if (sig_eq(g_tables[i]->sig, sig4)) {
      if (found == n) return g_tables[i];
      found++;
    }
  }
  return NULL;
}

ulong acpi_get_dsdt_phys(void) {
  return g_dsdt_phys;
}
