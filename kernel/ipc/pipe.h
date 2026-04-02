#pragma once
#include "types.h"

#define PIPE_BUF_SIZE 4096

typedef struct pipe {
  ubyte buf[PIPE_BUF_SIZE];
  uint read_pos;
  uint write_pos;
  uint count;
  int ref_count;
} pipe;

pipe *pipe_create(void);
void pipe_retain(pipe *p);
void pipe_release(pipe *p);

void pipe_write(pipe *p, const ubyte *data, uint len);
uint pipe_read(pipe *p, ubyte *data, uint max_len);
uint pipe_available(pipe *p);
