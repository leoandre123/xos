#include "filesystem.h"
#include "boot_info.h"
#include "filesystem/fat32.h"
#include "io/ata.h"
#include "io/ide.h"
#include "io/logging.h"
#include "io/pci.h"
#include "io/xhci.h"

disk g_disks[FILESYSTEM_MAX_DISKS];
static bool g_disk_used[FILESYSTEM_MAX_DISKS] = {0};

static disk *allocate_disk() {
  for (int i = 0; i < FILESYSTEM_MAX_DISKS; i++) {
    if (!g_disk_used[i]) {
      g_disk_used[i] = true;
      return &g_disks[i];
    }
  }
  return 0;
}
static void free_disk(disk *disk) {
  int idx = disk - g_disks;
  if (idx >= 0 && idx < FILESYSTEM_MAX_DISKS) {
    disk->id = 0;
    g_disk_used[idx] = false;
  }
}

void on_ide_found(ubyte bus, ubyte dev, ubyte func) {
  klogf(LOG_INFO, "IDE Disk found");
  disk *disk = allocate_disk();
  if (!disk) {
    klogf(LOG_WARNING, "Disk limit exceeded!");
    return;
  }
  disk->type = DISK_IDE;
  disk->ide.bus = bus;
  disk->ide.dev = dev;
  disk->ide.func = func;

  ide_init_controller(bus, dev, func);
}
void on_sata_found(ubyte bus, ubyte dev, ubyte func) {
  klogf(LOG_INFO, "IDE Disk found");
  disk *disk = allocate_disk();
}
void on_nvme_found(ubyte bus, ubyte dev, ubyte func) {
  klogf(LOG_INFO, "IDE Disk found");
  disk *disk = allocate_disk();
  disk->type = DISK_NVME;
  disk->nvme.bus = bus;
  disk->nvme.dev = dev;
  disk->nvme.func = func;
}

static void run_disk_search() {
  pci_scan_for_class(0x01, 0x01, on_ide_found);
  pci_scan_for_class(0x01, 0x06, on_sata_found);
  pci_scan_for_class(0x01, 0x08, on_nvme_found);
}

int fs_init(BootDevice *boot_device) {
  if (boot_device->type == BOOT_DEVICE_IDE) {
    bool ata_ok = ata_init();
    if (ata_ok) {
      klogf(LOG_INFO, "ATA initialized...");
      fat32_set_block_read(ata_read);
      ubyte gpt_buf[512];
      if (ata_read(2, 1, gpt_buf)) {
        uint part2_lba = *(uint *)(gpt_buf + 128 + 32);
        klogf(LOG_INFO, "ATA data partition LBA: %u", part2_lba);
        fat32_init(part2_lba);
      } else {
        klogf(LOG_WARNING, "Failed to read GPT from ATA, falling back to LBA 0");
        fat32_init(0);
      }
      klogf(LOG_INFO, "FAT32 initialized...");
      fat32_print_root();
      return 0;
    } else {
      klogf(LOG_WARNING, "No ATA drive, skipping FAT32");
      return 1;
    }
  } else if (boot_device->type == BOOT_DEVICE_USB) {
    if (usb_msc_ok()) {
      klogf(LOG_INFO, "USB MSC ready, parsing GPT...");
      ubyte gpt_buf[512];
      if (usb_msc_read(2, 1, gpt_buf)) {
        // GPT partition entries start at LBA 2; each entry is 128 bytes.
        // Partition 2 (index 1) starts at byte offset 128; LBA start is at +32.
        uint part2_lba = *(uint *)(gpt_buf + 128 + 32);
        klogf(LOG_INFO, "USB data partition LBA: %u", part2_lba);
        fat32_set_block_read(usb_msc_read);
        fat32_init(part2_lba);
        klogf(LOG_INFO, "FAT32 on USB initialized");
        fat32_print_root();
        return 0;
      } else {
        klogf(LOG_WARNING, "Failed to read GPT partition table from USB");
        return 1;
      }
    }
    return 1;
  }
  return 1;
}