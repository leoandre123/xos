#pragma once

#include "types.h"
ubyte inb(ushort port);
ushort inw(ushort port);
void outb(ushort port, ubyte value);
void io_wait(void);