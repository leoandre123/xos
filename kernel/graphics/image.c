#include "image.h"
#include "filesystem/file.h"
#include "memory/heap.h"

bitmap *img_load(const char *path) {
  file_handle h = file_open(path);
  bitmap *bm = kmalloc(h->size);
  file_read(h, bm, h->size);
  file_close(h);
  return bm;
}