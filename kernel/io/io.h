#pragma once

#include "types.h"
ubyte inb(ushort port);
ushort inw(ushort port);
void outb(ushort port, ubyte value);

void outl(ushort port, uint value);
uint inl(ushort port);

void io_wait(void);