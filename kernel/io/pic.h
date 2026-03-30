#pragma once

#include "types.h"
void pic_remap(int offset1, int offset2);
void pic_send_eoi(ubyte irq);
void pic_mask_all(void);
void pic_unmask_irq(ubyte irq);