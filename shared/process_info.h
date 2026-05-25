#pragma once
#include "types.h"

#define PROCESS_MAX_NAME_LENGTH 32

typedef enum : ubyte { PROC_READY,
                       PROC_RUNNING,
                       PROC_BLOCKED,
                       PROC_DEAD } proc_state;

typedef struct {
  uint pid;
  char name[PROCESS_MAX_NAME_LENGTH];
  proc_state state;
} process_info;