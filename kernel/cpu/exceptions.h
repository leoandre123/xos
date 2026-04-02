#pragma once

#define EX_DIVSION_ERROR            0x00
#define EX_DEBUG                    0x01
#define EX_NMI                      0x02
#define EX_BREAKPOINT               0x03
#define EX_OVERFLOW                 0x04
#define EX_BOUND_RANGE              0x05
#define EX_INVALID_OPCODE           0x06
#define EX_DEVICE_NOT_AVAILABLE     0x07
#define EX_DOUBLE_FALT              0x08
#define EX_INVALID_TSS              0x0A
#define EX_SEGMENT_NOT_PRESENT      0x0B
#define EX_STACK_SEGMENT_FAULT      0x0C
#define EX_GENERAL_PROTECTION_FAULT 0x0D
#define EX_PAGE_FAULT               0x0E

void exceptions_init();