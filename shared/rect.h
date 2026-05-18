#pragma once
#include "types.h"
typedef struct {
  short x;
  short y;
  ushort w;
  ushort h;
} rect;

// Union two rects into the smallest rect containing both
static rect rect_union(rect a, rect b) {
  if (a.w == 0)
    return b;
  if (b.w == 0)
    return a;
  int x2 = a.x + (int)a.w > b.x + (int)b.w ? a.x + (int)a.w : b.x + (int)b.w;
  int y2 = a.y + (int)a.h > b.y + (int)b.h ? a.y + (int)a.h : b.y + (int)b.h;
  rect r;
  r.x = a.x < b.x ? a.x : b.x;
  r.y = a.y < b.y ? a.y : b.y;
  r.w = (uint)(x2 - r.x);
  r.h = (uint)(y2 - r.y);
  return r;
}

// Intersection of two rects — returns zero-size rect if no overlap
static rect rect_intersect(rect a, rect b) {
  int x1 = a.x > b.x ? a.x : b.x;
  int y1 = a.y > b.y ? a.y : b.y;
  int x2 = a.x + (int)a.w < b.x + (int)b.w ? a.x + (int)a.w : b.x + (int)b.w;
  int y2 = a.y + (int)a.h < b.y + (int)b.h ? a.y + (int)a.h : b.y + (int)b.h;
  if (x2 <= x1 || y2 <= y1)
    return (rect){0};
  return (rect){(short)x1, (short)y1, (ushort)(x2 - x1), (ushort)(y2 - y1)};
}

static inline bool rect_equals(rect a, rect b) {
  return a.x == b.x &&
         a.y == b.y &&
         a.w == b.w &&
         a.h == b.h;
}