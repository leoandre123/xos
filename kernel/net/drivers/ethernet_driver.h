#pragma once

#include "net_types.h"
#include "types.h"

typedef struct {
  void (*init)(ulong mmio_phys);
  void (*poll)();
  void (*get_mac)(mac_addr *mac_out);
  void (*send)(void *data, ushort len);

} ed_ops;
