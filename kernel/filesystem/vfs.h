#pragma once
#include "boot_info.h"
#include "filesystem/file.h"
#include "filesystem/filesystem.h"
#include "types.h"

/*
    Virtual File System
*/

// Initilized the virtual file system and mounts the boot device at /
bool vfs_init(BootDevice *dev);

int vfs_open(const char *path, file_handle handle);
void vfs_close(file_handle handle);

int vfs_readdir(const char *path, file_dirent *out, int max);
int vfs_mkdir(const char *path, bool recursive);

int vfs_read(file_handle handle, void *buf, uint count);
int vfs_write(file_handle handle, void *data, uint count);

// Mount a disk at the specified location
bool vfs_mount(disk_id dev, const char *mnt_pt);