#pragma once

#define RING_BUF(name, type, cap)                       \
  typedef struct {                                      \
    type items[cap];                                    \
    int head, tail;                                     \
  } name;                                               \
  static inline int name##_write(name *q, type *item) { \
    if ((q->head + 1) % (cap) == q->tail)               \
      return 0;                                         \
    q->items[q->head] = *item;                          \
    q->head = (q->head + 1) % (cap);                    \
    return 1;                                           \
  }                                                     \
  static inline int name##_read(name *q, type *out) {   \
    if (q->head == q->tail)                             \
      return 0;                                         \
    *out = q->items[q->tail];                           \
    q->tail = (q->tail + 1) % (cap);                    \
    return 1;                                           \
  }                                                     \
  static inline int name##_empty(name *q) {             \
    return q->head == q->tail;                          \
  }                                                     \
  static inline int name##_full(name *q) {              \
    return (q->head + 1) % (cap) == q->tail;            \
  }
