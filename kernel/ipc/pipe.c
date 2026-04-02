#include "pipe.h"
#include "io/serial.h"
#include "memory/heap.h"

pipe *pipe_create(void) {
  pipe *p = kmalloc(sizeof(pipe));
  if (!p)
    return 0;
  p->read_pos = 0;
  p->write_pos = 0;
  p->count = 0;
  p->ref_count = 1;
  return p;
}

void pipe_retain(pipe *p) {
  p->ref_count++;
}

void pipe_release(pipe *p) {
  p->ref_count--;
  if (p->ref_count <= 0)
    kfree(p);
}

uint pipe_available(pipe *p) {
  return p->count;
}

void pipe_write(pipe *p, const ubyte *data, uint len) {
  for (uint i = 0; i < len; i++) {
    if (p->count >= PIPE_BUF_SIZE)
      break; // drop if full
    p->buf[p->write_pos] = data[i];
    p->write_pos = (p->write_pos + 1) % PIPE_BUF_SIZE;
    p->count++;
  }
}

uint pipe_read(pipe *p, ubyte *data, uint max_len) {
  uint i = 0;
  while (i < max_len && p->count > 0) {
    data[i++] = p->buf[p->read_pos];
    p->read_pos = (p->read_pos + 1) % PIPE_BUF_SIZE;
    p->count--;
  }
  return i;
}
