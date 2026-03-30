#pragma once
#include "types.h"
#include <stdbool.h>

bool ata_init(void);
bool ata_read(uint lba, ubyte count, void *buf);
