#pragma once
#include "types.h"

#define MAX_HANDLES 8

typedef enum {
  HANDLE_NONE = 0,
  HANDLE_PIPE_READ,
  HANDLE_PIPE_WRITE,
} handle_type;

typedef struct {
  handle_type type;
  void *ptr; // pipe *
} handle_entry;
