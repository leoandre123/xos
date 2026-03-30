#include "fat32.h"
#include "io/ata.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "types.h"
#include "utils/math.h"
#include <stdbool.h>

fat32_bpb g_bpb;

static char to_upper(char c) {
  return (c >= 'a' && c <= 'z') ? c - 0x20 : c;
}

static bool name_matches(ubyte *fat_name, const char *component, int len) {
  char padded[11];
  memset8((ubyte *)padded, ' ', 11);

  int i = 0, j = 0;
  while (j < len && component[j] != '.') {
    if (i >= 8)
      return false;
    padded[i++] = to_upper(component[j++]);
  }
  if (j < len && component[j] == '.') {
    j++; // skip dot
    i = 8;
    while (j < len) {
      if (i >= 11)
        return false;
      padded[i++] = to_upper(component[j++]);
    }
  }
  for (int k = 0; k < 11; k++)
    if (fat_name[k] != padded[k])
      return false;
  return true;
}

/*
foo == foo/
*/
static bool path_matches(ubyte *str1, ubyte *str2, int len) {
  for (int i = 0; i < len; i++) {
    if (to_upper(str1[i]) != to_upper(str2[i])) {
      return false;
    }
    if (str1[i] == '\0') {
      return true;
    }
  }
  return true;
}

static inline uint cluster_to_sector(uint cluster) {
  uint data_start = g_bpb.reserved_sectors + g_bpb.fat_count * g_bpb.sectors_per_fat_32;
  return data_start + (cluster - 2) * g_bpb.sectors_per_cluster;
}

static uint next_cluster(uint cluster) {
  uint lba = g_bpb.reserved_sectors + ((cluster * 4) / 512);
  uint offset = (cluster % (128)) * 4;
  ubyte buf[512];
  ata_read(lba, 1, &buf);
  return *(uint *)(buf + offset) & 0x0FFFFFFF;
}

static void parse_lfn_name(fat32_lfn_entry *entry, char *buf) {

  buf[0] = (ubyte)entry->name_0[0];
  buf[1] = (ubyte)entry->name_0[1];
  buf[2] = (ubyte)entry->name_0[2];
  buf[3] = (ubyte)entry->name_0[3];
  buf[4] = (ubyte)entry->name_0[4];
  buf[5] = (ubyte)entry->name_1[0];
  buf[6] = (ubyte)entry->name_1[1];
  buf[7] = (ubyte)entry->name_1[2];
  buf[8] = (ubyte)entry->name_1[3];
  buf[9] = (ubyte)entry->name_1[4];
  buf[10] = (ubyte)entry->name_1[5];
  buf[11] = (ubyte)entry->name_2[0];
  buf[12] = (ubyte)entry->name_2[1];
}

static void read_directory(uint cluster) {
}
static void read_file(uint cluster) {
}

void read_BPB(void) {
}

fat32_file *fat32_open(const char *path) {
  uint current_cluster = g_bpb.root_cluster;

  fat32_directory_entry entries[16];

  /*
   *
   * /foo/bar
   *  ^  ^ ^ ^
   */

  char name_buf[255];
  const char *path_start = path + 1;
  bool found_dir = false;

  // Loop every path part
  while (1) {
    const char *path_end = path_start + 1;
    while (*path_end != '/' && *path_end != '\0') {
      path_end++;
    }

    found_dir = false;

    // Loop every cluster of directory
    while (current_cluster < 0x0FFFFFF8) {
      uint sector = cluster_to_sector(current_cluster);

      // Loop every sector of cluster
      for (uint i = 0; i < g_bpb.sectors_per_cluster; i++) {
        ata_read(sector + i, 1, &entries);

        // Loop every entry of sector
        for (uint j = 0; j < 16; j++) {
          if (entries[j].name[0] == 0x00) // Last entry -> file not found
            return 0;
          if (entries[j].name[0] == 0xE5) // Deleted entry -> skip
            continue;
          if (entries[j].attributes == FAT32_ATTR_LFN) {
            fat32_lfn_entry *lfn_entry = (fat32_lfn_entry *)(entries + j);
            ubyte seq = lfn_entry->sequence_number & FAT32_LFN_SEQUENCE_MASK;
            if (lfn_entry->sequence_number & FAT32_LFN_ENTRY_LAST) {
              name_buf[seq * 13] = '\0';
            }
            parse_lfn_name(lfn_entry, name_buf + ((seq - 1) * 13));
            continue;
          }
          if (entries[j].attributes & FAT32_ATTR_VOLUME)
            continue;

          int comp_len = path_end - path_start;
          bool matches = path_matches((ubyte *)path_start, (ubyte *)name_buf, comp_len) ||
                         name_matches(entries[j].name, path_start, comp_len);
          if (matches) {
            if (*path_end == '\0') {
              fat32_file *handle = kmalloc(sizeof(fat32_file));
              handle->size = entries[j].size;
              handle->first_cluster = (((uint)entries[j].cluster_high) << 0x10) | entries[j].cluster_low;
              return handle;
            } else {
              current_cluster = (((uint)entries[j].cluster_high) << 0x10) | entries[j].cluster_low;
              found_dir = true;
              break;
            }
          }
        }
        if (found_dir)
          break;
      }
      if (found_dir)
        break;

      current_cluster = next_cluster(current_cluster);
    }

    if (!found_dir)
      return 0;
    path_start = path_end + 1;
  }
}

uint fat32_read(fat32_file *file, void *buf, uint count) {
  ubyte *dst = buf;
  uint bytes_read = 0;
  uint cluster_size = g_bpb.sectors_per_cluster * 512;

  ubyte *tmp = kmalloc(cluster_size); // heap, not stack
  uint cluster = file->first_cluster;

  while (cluster < 0x0FFFFFF8 && bytes_read < count) {
    uint lba = cluster_to_sector(cluster);
    ata_read(lba, g_bpb.sectors_per_cluster, tmp);

    uint remaining = count - bytes_read;
    uint to_copy = remaining < cluster_size ? remaining : cluster_size;
    memcpy8(dst + bytes_read, tmp, to_copy);
    bytes_read += to_copy;

    cluster = next_cluster(cluster);
  }

  kfree(tmp);
  return bytes_read;
}

fat32_file *fat32_list_files(const char *path, int *count_read) {
  uint current_cluster = g_bpb.root_cluster;

  fat32_directory_entry entries[16];

  bool break_out = 0;

  /*
   *
   * /foo/bar
   *  ^  ^ ^ ^
   */

  char name_buf[255];
  const char *path_start = path + 1;

  // Loop every path part
  while (1) {
    const char *path_end = path_start + 1;
    while (*path_end != '/' && *path_end != '\0') {
      path_end++;
    }

    // Loop every cluster of directory
    while (current_cluster < 0x0FFFFFF8) {
      uint sector = cluster_to_sector(current_cluster);

      // Loop every sector of cluster
      for (uint i = 0; i < g_bpb.sectors_per_cluster; i++) {
        ata_read(sector + i, 1, &entries);

        // Loop every entry of sector
        for (uint j = 0; j < 16; j++) {
          if (entries[j].name[0] == 0x00) // Last entry -> file not found
            return 0;
          if (entries[j].name[0] == 0xE5) // Deleted entry -> skip
            continue;
          if (entries[j].attributes == FAT32_ATTR_LFN) {
            fat32_lfn_entry *lfn_entry = (fat32_lfn_entry *)(entries + j);
            ubyte seq = lfn_entry->sequence_number & FAT32_LFN_SEQUENCE_MASK;
            if (lfn_entry->sequence_number & FAT32_LFN_ENTRY_LAST) {
              name_buf[seq * 13] = '\0';
            }
            parse_lfn_name(lfn_entry, name_buf + ((seq - 1) * 13));
            continue;
          }
          if (entries[j].attributes & FAT32_ATTR_VOLUME)
            continue;

          if (path_matches((ubyte *)path_start, (ubyte *)name_buf, (path_end - path_start))) {
            if (*path_end == '\0') {
              fat32_file *handle = kmalloc(sizeof(fat32_file));
              handle->size = entries[j].size;
              handle->first_cluster = (((uint)entries[j].cluster_high) << 0x10) | entries[j].cluster_low;
              return handle;
            } else {
              break_out = 1;
              current_cluster = (((uint)entries[j].cluster_high) << 0x10) | entries[j].cluster_low;
              break;
            }
          }
        }
        if (break_out)
          break;
      }

      if (break_out)
        break;
      current_cluster = next_cluster(current_cluster);
    }
    break_out = false;
    path_start = path_end + 1;
  }
}

int fat32_init(uint lba_start) {
  ata_read(lba_start, 1, &g_bpb);
}

void fat32_print_root(void) {
  fat32_directory_entry entries[16];

  serial_write_line("root:");
  uint current_cluster = g_bpb.root_cluster;

  char name_buf[255];
  while (current_cluster) {
    uint sector = cluster_to_sector(current_cluster);
    for (uint i = 0; i < g_bpb.sectors_per_cluster; i++) {
      ata_read(sector + i, 1, &entries);
      for (uint j = 0; j < 16; j++) {
        if (entries[j].name[0] == 0x00)
          return;
        serial_write_char('*');
        if (entries[j].name[0] == 0xE5)
          serial_write("[DELETED]");
        if (entries[j].attributes == FAT32_ATTR_LFN) {
          fat32_lfn_entry *lfn_entry = (fat32_lfn_entry *)(entries + j);
          ubyte seq = lfn_entry->sequence_number & FAT32_LFN_SEQUENCE_MASK;
          if (lfn_entry->sequence_number & FAT32_LFN_ENTRY_LAST) {
            name_buf[seq * 13] = '\0';
          }
          parse_lfn_name(lfn_entry, name_buf + ((seq - 1) * 13));
          if (seq == 1)
            serial_write(name_buf);
          continue;
        }

        serial_write_hex8(entries[j].attributes);
        serial_write_char(' ');
        serial_write_bin8(entries[j].name[0]);
        serial_write_char(' ');
        for (int i = 0; i < 11; i++) {
          serial_write_char(entries[j].name[i]);
        }
        serial_write_char('\n');
      }
    }

    current_cluster = next_cluster(current_cluster);
  }
}
