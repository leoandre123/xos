#include "file.h"
#include "filesystem/fat32.h"

static file file_pool[MAX_OPEN_FILES];
static bool file_pool_used[MAX_OPEN_FILES];

static inline file_handle alloc_handle() {
  for (int i = 0; i < MAX_OPEN_FILES; i++) {
    if (!file_pool_used[i]) {
      file_pool_used[i] = true;
      return &file_pool[i];
    }
  }
  return 0;
}
static inline void free_handle(file_handle handle) {
  int idx = handle - file_pool;
  if (idx >= 0 && idx < MAX_OPEN_FILES) {
    file_pool_used[idx] = false;
  }
}

static const fs_ops *fs_lookup(const char *path) {
  (void)path;
  return &g_fat32_ops;
}
static const fs_ops *fs_get_ops(fs_type type) {
  (void)type;
  return &g_fat32_ops;
}

file_handle file_open(const char *path) {
  file_handle handle = alloc_handle();
  if (!handle)
    return 0;

  const fs_ops *ops = fs_lookup(path);

  ops->open(path, handle);

  return handle;
}
void file_close(file_handle handle) {
  if (!handle)
    return;
  const fs_ops *ops = fs_get_ops(handle->fs_type);
  ops->close(handle);
  free_handle(handle);
}

int file_readdir(const char *path, file_dirent *out, int max) {
  const fs_ops *ops = fs_lookup(path);
  return ops->readdir(path, out, max);
}

uint file_read(file_handle handle, void *buf, uint count) {
  const fs_ops *ops = fs_get_ops(handle->fs_type);
  return ops->read(handle, buf, count);
}