#pragma once
#include "types.h"

#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME    0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LFN       0x0F

#define FAT32_LFN_ENTRY_LAST    0x40
#define FAT32_LFN_ENTRY_DELETED 0x80
#define FAT32_LFN_SEQUENCE_MASK 0x1F

typedef struct {
  ubyte jump[3];     // jump instruction (ignore)
  ubyte oem_name[8]; // OEM name (ignore)

  // DOS 2.0 BPB
  ushort bytes_per_sector; // almost always 512
  ubyte sectors_per_cluster;
  ushort reserved_sectors; // sectors before FAT (includes this sector)
  ubyte fat_count;         // number of FATs (usually 2)
  ushort root_entry_count; // 0 for FAT32
  ushort total_sectors_16; // 0 for FAT32
  ubyte media_type;
  ushort sectors_per_fat_16; // 0 for FAT32
  ushort sectors_per_track;
  ushort head_count;
  uint hidden_sectors;
  uint total_sectors_32;

  // FAT32 extended BPB
  uint sectors_per_fat_32;
  ushort flags;
  ushort version;
  uint root_cluster; // cluster number of root directory (usually 2)
  ushort fsinfo_sector;
  ushort backup_boot_sector;
  ubyte reserved[12];
  ubyte drive_number;
  ubyte reserved2;
  ubyte boot_signature; // 0x29 if next three fields are valid
  uint volume_id;
  ubyte volume_label[11];
  ubyte fs_type[8]; // "FAT32   "
  ubyte boot_code[420];
  ubyte bootable_partition_signature;
} __attribute__((packed)) fat32_bpb;

typedef struct {
  ubyte name[11];
  ubyte attributes;
  ubyte reserved;
  ubyte creation_time_ms;
  ushort creation_time;
  ushort creation_date;
  ushort last_access_date;
  ushort cluster_high;
  ushort last_modification_time;
  ushort last_modification_date;
  ushort cluster_low;
  uint size;
} __attribute__((packed)) fat32_directory_entry;

typedef struct {
  ubyte sequence_number;
  ushort name_0[5];
  ubyte attributes;
  ubyte type;
  ubyte checksum;
  ushort name_1[6];
  ushort first_cluster;
  ushort name_2[2]
} __attribute__((packed)) fat32_lfn_entry;

typedef struct {
  uint first_cluster;
  uint size;
} fat32_file;

int fat32_init(uint lba_start);
fat32_file *fat32_open(const char *path);
int fat32_write(const char *path, ubyte *buf);
void fat32_print_root(void);
uint fat32_read(fat32_file *file, void *buf, uint count);