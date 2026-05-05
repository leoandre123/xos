#include "types.h"
typedef enum {
  DISK_IDE,
  DISK_ATA,
  DISK_NVME,
  DISK_USB
} disk_type;

typedef int disk_id;

typedef struct {
  disk_id id;
  disk_type type;
  ulong sector_count;
  ulong sector_size;

  bool is_boot_disk;

  union {
    struct {
      ubyte bus, dev, func;
      ubyte channel;
      ubyte drive; // master/slave
    } ide;
    struct {
      ubyte bus, dev, func;
      ubyte port;
    } ahci;

    struct {
      ubyte bus, dev, func;
      uint namespace_id;
    } nvme;
  };
} disk;