#pragma once
#include "types.h"

#define PROCESS_MAX_NAME_LENGTH 32

typedef struct {
  uint pid;
  char name[PROCESS_MAX_NAME_LENGTH];
} process_info;