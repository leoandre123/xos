#pragma once
#include "types.h"
#include <stdint.h>

#define KERNEL_CODE_SEL 0x08
#define KERNEL_DATA_SEL 0x10
#define USER_CODE_SEL   0x1B
#define USER_DATA_SEL   0x23
#define TSS_SEL         0x28

void jump_to_userspace(ulong entry, ulong user_rsp);
void gdt_init(void);
void gdt_set_kernel_stack(ulong rsp0);