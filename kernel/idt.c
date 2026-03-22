#include "idt.h"

extern void isr0(void);
extern void isr3(void);
extern void isr13(void);
extern void isr14(void);

static struct idt_entry idt[256];
static struct idt_ptr idtr;

static uint16_t read_cs(void)
{
    uint16_t cs;
    asm volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static void idt_set_gate(int vector, void *isr, uint8_t flags)
{
    uint64_t addr = (uint64_t)isr;

    idt[vector].offset_low = addr & 0xFFFF;
    idt[vector].selector = 0x08; // kernel code segment
    idt[vector].selector = read_cs();
    idt[vector].ist = 0x00;
    idt[vector].type_attr = flags;
    idt[vector].offset_mid = (addr >> 16) & 0xFFFF;
    idt[vector].offset_high = (addr >> 32) & 0xFFFFFFFF;
    idt[vector].zero = 0;
}

static void idt_load(struct idt_ptr *ptr)
{
    asm volatile("lidt (%0)" : : "r"(ptr));
}

uint16_t idt_init(void)
{
    uint16_t loc = read_cs();
    for (int i = 0; i < 256; i++)
    {
        idt_set_gate(i, 0, 0);
    }

    // 0x8E = present | interrupt gate | ring 0
    idt_set_gate(0, isr0, 0x8E);
    idt_set_gate(3, isr3, 0x8E);
    idt_set_gate(13, isr13, 0x8E);
    idt_set_gate(14, isr14, 0x8E);

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint64_t)&idt[0];

    idt_load(&idtr);

    return loc;
}