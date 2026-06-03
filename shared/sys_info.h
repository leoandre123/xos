#pragma once

typedef enum {
  SYSINFO_KERNEL,
  SYSINFO_MEMORY,
  SYSINFO_CPU,
  SYSINFO_PROCESS,
} sys_info_type;

typedef struct {
  char name[32];
  char version[32];
} kernel_info;