#include "vfs.h"
#include "filesystem/file.h"
#include "filesystem/filesystem.h"
#include "scheduler/scheduler.h"
#include <string.h>

#define MAX_MOUNTS 32

typedef struct {
  disk_id drive;
  char *mnt_pt;
  fs_type type;
} mount;

static mount s_mounts[MAX_MOUNTS];

static inline int str_starts_with(const char *s, const char *prefix) {
  while (*prefix)
    if (*s++ != *prefix++)
      return 0;
  return 1;
}

static int resolve_disk(const char *path, const char *wd, disk_id *disk_out, const char **path_out) {
  bool relative = path[0] != '/';

  int best_len = 0;
  int best_idx = -1;
  if (!relative) {
    for (int i = 0; i < MAX_MOUNTS; i++) {
      if (s_mounts[i].mnt_pt) {
        int len = strlen(s_mounts[i].mnt_pt);
        if (len > best_len && str_starts_with(path, s_mounts[i].mnt_pt)) {
          best_idx = i;
          best_len = len;
        }
      }
    }
  }
  if (best_idx == -1)
    return -1;
  *disk_out = s_mounts[best_idx].drive;
  *path_out = (path + best_len);
  return 0;
}

int vfs_readdir(const char *path, file_dirent *out, int max) {
  process *p = process_current();
  disk_id did;
  resolve_path(path, p->working_directory, &did);

  disk *disk = get_disk(did);

  if (!disk)
    return -1;
}