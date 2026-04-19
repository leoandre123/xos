#pragma once
#include "types.h"

#define MAX_OPEN_FILES 16

typedef enum {
  FAT_32
} fs_type;

typedef struct {
  fs_type fs_type;
  void *priv;
  uint size;
} file;

typedef file *file_handle;

typedef struct {
  char name[255];
  bool is_dir;
} file_dirent;

typedef struct {
  int (*open)(const char *path, file_handle handle);
  void (*close)(file_handle handle);
  int (*readdir)(const char *path, file_dirent *out, int max);
  uint (*read)(file_handle handle, void *buf, uint count);
} fs_ops;

file_handle file_open(const char *path);
void file_close(file_handle handle);
int file_readdir(const char *path, file_dirent *out, int max);
uint file_read(file_handle handle, void *buf, uint count);