#pragma once

#include "types.h"
ubyte inb(ushort port);
ushort inw(ushort port);
void outb(ushort port, ubyte value);
void outw(ushort port, ushort value);

void outl(ushort port, uint value);
uint inl(ushort port);

void io_wait(void);