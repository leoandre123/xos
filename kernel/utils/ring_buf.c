// #include "ring_buf.h"
// #include "memory/memutils.h"
//
// int ring_buf_write(void *buf, int element_size, int length, int *head, int *tail, void *data, int len) {
//   if ((*head + 1) % length == *tail) {
//     return 0;
//   }
//   memcpy8(&((ubyte *)buf)[*head * element_size], data, len);
//   *head = (*head + 1) % length;
//   return 1;
// }
// int ring_buf_read(void *buf, int element_size, int length, int *head, int *tail, void *data, int max_len) {
//   if (*head == *tail) {
//     return 0;
//   }
//
//   memcpy8(data, &((ubyte *)buf)[*tail * element_size], max_len);
//
//   *tail = (*tail + 1) % length;
//   return 1;
// }