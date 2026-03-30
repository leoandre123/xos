#include "ata.h"
#include "io/io.h"
#include "serial.h"
#include <stdbool.h>

#define ATA_DATA         0x1F0
#define ATA_ERROR        0x1F1
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW      0x1F3
#define ATA_LBA_MID      0x1F4
#define ATA_LBA_HIGH     0x1F5
#define ATA_DRIVE        0x1F6
#define ATA_STATUS       0x1F7
#define ATA_COMMAND      0x1F7

#define ATA_STATUS_BSY 0x80 // drive busy
#define ATA_STATUS_DRQ 0x08 // data ready
#define ATA_STATUS_ERR 0x01 // error

#define ATA_CMD_READ 0x20

static bool ata_wait(void) {
  ubyte status;
  // wait for BSY to clear
  do {
    status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR)
      return false;
  } while (status & ATA_STATUS_BSY);

  // wait for DRQ to set
  do {
    status = inb(ATA_STATUS);
    if (status & ATA_STATUS_ERR)
      return false;
  } while (!(status & ATA_STATUS_DRQ));

  return true;
}

bool ata_init(void) {
  // check if a drive exists by reading status
  ubyte status = inb(ATA_STATUS);
  if (status == 0xFF) {
    serial_write_line("ATA: no drive detected");
    return false;
  }
  serial_write_line("ATA: drive detected");
  return true;
}

bool ata_read(uint lba, ubyte count, void *buf) {
  // select master drive, LBA mode, bits 24-27 of LBA
  outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_SECTOR_COUNT, count);
  outb(ATA_LBA_LOW, (lba) & 0xFF);
  outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
  outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
  outb(ATA_COMMAND, ATA_CMD_READ);

  ushort *dst = buf;
  for (int s = 0; s < count; s++) {
    if (!ata_wait())
      return false;

    // read 256 words = 512 bytes per sector
    for (int i = 0; i < 256; i++)
      dst[i] = inw(ATA_DATA);

    dst += 256;
  }
  return true;
}
