#include <stdint.h>
#include "gdt.h"

struct gdt_ptr
{
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static uint64_t gdt[3];
static struct gdt_ptr gdtr;

extern void gdt_load_flush(uint64_t gdtr_addr, uint16_t data_selector);

void gdt_init(void)
{
    // Null descriptor
    gdt[0] = 0x0000000000000000ULL;

    // Kernel code segment: base=0, limit ignored in long mode
    // Access = 0x9A, Flags = 0xA
    gdt[1] = 0x00AF9A000000FFFFULL;

    // Kernel data segment
    // Access = 0x92, Flags = 0xA
    gdt[2] = 0x00AF92000000FFFFULL;

    gdtr.limit = sizeof(gdt) - 1;
    gdtr.base = (uint64_t)&gdt[0];

    gdt_load_flush((uint64_t)&gdtr, 0x10);
}