#pragma once
#define MAX_HANDLES 128

struct process;

typedef enum {
  HANDLE_NONE = 0,
  HANDLE_PIPE_READ,
  HANDLE_PIPE_WRITE,
  HANDLE_FILE,
  HANDLE_CHANNEL
} handle_type;

typedef struct {
  handle_type type;
  void *ptr;
} handle_entry;

typedef int base_handle;
typedef base_handle pipe_read_handle;
typedef base_handle pipe_write_handle;
// typedef base_handle file_handle;
typedef base_handle channel_handle;

base_handle handle_alloc(struct process *p, handle_type type, void *ptr);
handle_entry *handle_get(struct process *p, base_handle h);