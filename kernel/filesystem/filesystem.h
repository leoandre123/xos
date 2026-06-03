#pragma once
#include "boot_info.h"
#include "disk.h"

#define FILESYSTEM_MAX_DISKS 32

extern disk g_disks[FILESYSTEM_MAX_DISKS];

int fs_init(BootDevice *boot_device);

static inline disk *get_disk(disk_id id) {
  for (int i = 0; i < FILESYSTEM_MAX_DISKS; i++) {
    if (g_disks[i].id == id) {
      return &g_disks[i];
    }
  }
  return 0;
}