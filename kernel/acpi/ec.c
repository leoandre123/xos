#include "ec.h"
#include "acpi/acpi.h"
#include "io/io.h"
#include "io/logging.h"

// EC status/command register bits
#define EC_OBF (1 << 0) // Output Buffer Full  — data ready to read from data port
#define EC_IBF (1 << 1) // Input Buffer Full   — EC busy, wait before writing

// EC commands
#define EC_CMD_READ  0x80
#define EC_CMD_WRITE 0x81

static ushort ec_cmd_port  = 0x66;
static ushort ec_data_port = 0x62;
static bool   ec_present   = false;

// Generic Address Structure layout (ACPI GAS, 12 bytes):
//   [0]  address_space_id
//   [1]  register_bit_width
//   [2]  register_bit_offset
//   [3]  access_size
//   [4..11] address (64-bit)
#define GAS_ADDR_OFF 4

void ec_init(void) {
  acpi_header *ecdt = acpi_find_table("ECDT");
  if (ecdt) {
    ubyte *p = (ubyte *)ecdt;
    // ECDT layout: header (36 bytes), ec_control GAS (12 bytes), ec_data GAS (12 bytes)
    ulong ctrl_addr = *(ulong *)(p + 36 + GAS_ADDR_OFF);
    ulong data_addr = *(ulong *)(p + 48 + GAS_ADDR_OFF);
    ec_cmd_port  = (ushort)ctrl_addr;
    ec_data_port = (ushort)data_addr;
    klogf(LOG_INFO, "ACPI EC: ECDT ports cmd=0x%x data=0x%x", ec_cmd_port, ec_data_port);
  } else {
    klogf(LOG_INFO, "ACPI EC: no ECDT, using default ports 0x62/0x66");
  }
  ec_present = true;
}

static void ec_wait_ibf(void) {
  for (int i = 0; i < 100000; i++) {
    if (!(inb(ec_cmd_port) & EC_IBF))
      return;
    io_wait();
  }
}

static void ec_wait_obf(void) {
  for (int i = 0; i < 100000; i++) {
    if (inb(ec_cmd_port) & EC_OBF)
      return;
    io_wait();
  }
}

ubyte ec_read(ubyte reg) {
  if (!ec_present)
    return 0;
  ec_wait_ibf();
  outb(ec_cmd_port, EC_CMD_READ);
  ec_wait_ibf();
  outb(ec_data_port, reg);
  ec_wait_obf();
  return inb(ec_data_port);
}

void ec_write(ubyte reg, ubyte val) {
  if (!ec_present)
    return;
  ec_wait_ibf();
  outb(ec_cmd_port, EC_CMD_WRITE);
  ec_wait_ibf();
  outb(ec_data_port, reg);
  ec_wait_ibf();
  outb(ec_data_port, val);
}

ushort ec_read16(ubyte reg) {
  return (ushort)ec_read(reg) | ((ushort)ec_read(reg + 1) << 8);
}

uint ec_read32(ubyte reg) {
  return (uint)ec_read(reg) | ((uint)ec_read(reg + 1) << 8) |
         ((uint)ec_read(reg + 2) << 16) | ((uint)ec_read(reg + 3) << 24);
}
