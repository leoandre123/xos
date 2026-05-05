#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include "types.h"

#define BOOT_DEVICE_IDE  0
#define BOOT_DEVICE_USB  1
#define BOOT_DEVICE_AHCI 2
#define BOOT_DEVICE_NVME 3

typedef struct {
  ubyte type;     // BOOT_DEVICE_*
  ulong data_lba; // start LBA of OS data partition (0 = unknown/raw)
  union {
    struct {
      ubyte bus;
      ubyte dev;
      ubyte func;
    } pci; // IDE, AHCI, NVMe
    struct {
      ubyte port_path[8]; // hub port chain from root, 0-terminated
    } usb;
  };
} BootDevice;

typedef struct BootInfo {
  ulong framebuffer_base;
  uint framebuffer_width;
  uint framebuffer_height;
  uint framebuffer_pitch;
  ulong memory_map;
  ulong memory_map_size;
  ulong memory_map_desc_size;
  uint memory_map_desc_version;
  BootDevice boot_device;
} BootInfo;

#endif