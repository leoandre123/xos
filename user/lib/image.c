#include "image.h"
#include "fs/file.h"
#include "syscall.h"

bitmap *img_load(const char *path) {
  file_handle h = file_open(path);
  uint size = file_size(h);
  bitmap *bm = sys_alloc(size);
  file_read(h, bm, size);
  file_close(h);
  return bm;
}