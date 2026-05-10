#include "fat32.h"
#include "filesystem/file.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/memutils.h"
#include "types.h"

typedef bool (*block_read_fn)(uint lba, ubyte count, void *buf);
static block_read_fn g_block_read;

void fat32_set_block_read(block_read_fn fn) { g_block_read = fn; }

#define FAT32_MAX_OPEN_FILES 16

fat32_bpb g_bpb;
static uint g_lba_start;

fs_ops g_fat32_ops = {
    .open = fat32_open,
    .close = fat32_close,
    .readdir = fat32_readdir,
    .read = fat32_read,
};

static fat32_file file_pool[FAT32_MAX_OPEN_FILES] = {0};
static bool file_pool_used[FAT32_MAX_OPEN_FILES] = {0};

static inline fat32_file *alloc_file() {
  for (int i = 0; i < FAT32_MAX_OPEN_FILES; i++) {
    if (!file_pool_used[i]) {
      file_pool_used[i] = true;
      return &file_pool[i];
    }
  }
  return 0;
}
static inline void free_file(fat32_file *handle) {
  int idx = handle - file_pool;
  if (idx >= 0 && idx < FAT32_MAX_OPEN_FILES) {
    file_pool_used[idx] = false;
  }
}

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
  return g_lba_start + data_start + (cluster - 2) * g_bpb.sectors_per_cluster;
}

static uint next_cluster(uint cluster) {
  uint lba = g_lba_start + g_bpb.reserved_sectors + ((cluster * 4) / 512);
  uint offset = (cluster % (128)) * 4;
  ubyte buf[512];
  g_block_read(lba, 1, &buf);
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

static int open(const char *path, fat32_file *file) {

  if (path[0] == '/' && path[1] == '\0') {
    file->first_cluster = g_bpb.root_cluster;
    file->size = 0;
    file->is_dir = true;
    return 0;
  }

  uint current_cluster = g_bpb.root_cluster;

  fat32_directory_entry entries[16];

  /*
   *
   * /foo/bar
   *  ^  ^ ^ ^
   */

  char name_buf[255];
  bool has_lfn = false;
  const char *path_start = path + 1;
  bool found_dir = false;

  // Loop every path part
  while (1) {
    const char *path_end = path_start + 1;
    while (*path_end != '/' && *path_end != '\0') {
      path_end++;
    }

    found_dir = false;
    has_lfn = false;

    // Loop every cluster of directory
    while (current_cluster < 0x0FFFFFF8) {
      uint sector = cluster_to_sector(current_cluster);

      // Loop every sector of cluster
      for (uint i = 0; i < g_bpb.sectors_per_cluster; i++) {
        g_block_read(sector + i, 1, &entries);

        // Loop every entry of sector
        for (uint j = 0; j < 16; j++) {
          if (entries[j].name[0] == 0x00) // Last entry -> file not found
            return 1;
          if (entries[j].name[0] == 0xE5) { // Deleted entry -> skip
            has_lfn = false;
            continue;
          }
          if (entries[j].attributes == FAT32_ATTR_LFN) {
            fat32_lfn_entry *lfn_entry = (fat32_lfn_entry *)(entries + j);
            ubyte seq = lfn_entry->sequence_number & FAT32_LFN_SEQUENCE_MASK;
            if (lfn_entry->sequence_number & FAT32_LFN_ENTRY_LAST) {
              name_buf[seq * 13] = '\0';
            }
            parse_lfn_name(lfn_entry, name_buf + ((seq - 1) * 13));
            has_lfn = true;
            continue;
          }
          if (entries[j].attributes & FAT32_ATTR_VOLUME) {
            has_lfn = false;
            continue;
          }

          int comp_len = path_end - path_start;
          bool matches = (has_lfn && path_matches((ubyte *)path_start, (ubyte *)name_buf, comp_len)) ||
                         name_matches(entries[j].name, path_start, comp_len);
          has_lfn = false;
          if (matches) {
            if (*path_end == '\0') {
              file->is_dir = entries[j].attributes & FAT32_ATTR_DIRECTORY;
              file->size = entries[j].size;
              file->first_cluster = (((uint)entries[j].cluster_high) << 0x10) | entries[j].cluster_low;
              return 0;
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
      return 1;
    path_start = path_end + 1;
  }
}

int fat32_open(const char *path, file_handle handle) {
  fat32_file *file = alloc_file();
  if (!file)
    return -1;

  if (open(path, file) != 0) {
    free_file(file);
    return -1;
  }

  handle->priv = file;
  handle->size = file->size;
  return 0;
}
void fat32_close(file_handle handle) {
  free_file(handle->priv);
}

static uint read(fat32_file *file, void *buf, uint count) {
  ubyte *dst = buf;
  uint bytes_read = 0;
  uint cluster_size = g_bpb.sectors_per_cluster * 512;

  ubyte *tmp = kmalloc(cluster_size); // heap, not stack
  uint cluster = file->first_cluster;

  while (cluster < 0x0FFFFFF8 && bytes_read < count) {
    uint lba = cluster_to_sector(cluster);
    g_block_read(lba, g_bpb.sectors_per_cluster, tmp);

    uint remaining = count - bytes_read;
    uint to_copy = remaining < cluster_size ? remaining : cluster_size;
    memcpy8(dst + bytes_read, tmp, to_copy);
    bytes_read += to_copy;

    cluster = next_cluster(cluster);
  }

  kfree(tmp);
  return bytes_read;
}
uint fat32_read(file_handle handle, void *buf, uint count) {
  return read(handle->priv, buf, count);
}

int fat32_readdir(const char *path, file_dirent *out, int max) {
  serial_printf("fat32_readdir: %s\n", path);
  fat32_directory_entry entries[16];

  int count_read = 0;

  bool has_lfn = false;

  fat32_file handle;
  if (open(path, &handle)) {
    serial_printf("Cannot resolve path: %s\n", path);
    return 0;
  }
  if (!handle.is_dir) {
    serial_printf("Not a directory\n");
    return 0;
  }
  uint current_cluster = handle.first_cluster;
  while (current_cluster < 0x0FFFFFF8) {
    // Loop every sector of cluster
    uint sector = cluster_to_sector(current_cluster);
    for (uint i = 0; i < g_bpb.sectors_per_cluster; i++) {
      g_block_read(sector + i, 1, &entries);
      for (int j = 0; j < 16; j++) {
        if (entries[j].name[0] == 0x00) // Last entry -> file not found
        {
          return count_read;
        }
        if (entries[j].name[0] == 0xE5) // Deleted entry -> skip
          continue;

        if (entries[j].name[0] == '.') // Dot entry - skip
          continue;

        if (entries[j].attributes == FAT32_ATTR_LFN) {
          has_lfn = true;
          fat32_lfn_entry *lfn_entry = (fat32_lfn_entry *)(entries + j);
          ubyte seq = lfn_entry->sequence_number & FAT32_LFN_SEQUENCE_MASK;
          if (lfn_entry->sequence_number & FAT32_LFN_ENTRY_LAST) {
            out[count_read].name[seq * 13] = '\0';
          }
          parse_lfn_name(lfn_entry, (out[count_read].name) + ((seq - 1) * 13));
          continue;
        }
        out[count_read].file_size = entries[j].size;
        out[count_read].is_dir = entries[j].attributes & FAT32_ATTR_DIRECTORY;
        if (!has_lfn) {
          char *n = &(out[count_read].name[0]);
          for (int k = 0; k < 11; k++) {
            char ch = entries[j].name[k];
            if (ch != ' ')
              *n++ = ch;
            if (k == 7 && entries[j].name[8] != ' ')
              *n++ = '.';
          }
          *n = '\0';
        }
        serial_printf("- %s (%s)\n", entries[j].name, out[count_read].name);

        has_lfn = false;
        count_read++;
        if (count_read == max) {
          return count_read;
        }
      }
    }

    current_cluster = next_cluster(current_cluster);
  }

  return count_read;
}

int fat32_init(uint lba_start) {
  g_lba_start = lba_start;
  g_block_read(lba_start, 1, &g_bpb);
  return 0;
}

void fat32_print_root(void) {
  fat32_directory_entry entries[16];

  serial_write_line("root:");
  uint current_cluster = g_bpb.root_cluster;

  char name_buf[255];
  while (current_cluster) {
    uint sector = cluster_to_sector(current_cluster);
    for (uint i = 0; i < g_bpb.sectors_per_cluster; i++) {
      g_block_read(sector + i, 1, &entries);
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
