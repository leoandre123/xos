#pragma once
#include "boot_info.h"
#include "disk.h"

#define FILESYSTEM_MAX_DISKS 32

extern disk g_disks[FILESYSTEM_MAX_DISKS];

int fs_init(BootDevice *boot_device);