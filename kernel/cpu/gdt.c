#include "gdt.h"
#include "types.h"

struct gdt_ptr {
  ushort limit;
  ulong base;
} __attribute__((packed));

typedef struct {
  uint reserved0;
  ulong rsp0;
  ulong rsp1;
  ulong rsp2;
  ulong reserved1;
  ulong ist[7];
  ulong reserved2;
  ushort reserved3;
  ushort iomap_base;
} __attribute__((packed)) tss_t;

typedef struct {
  ushort limit_low;
  ushort base_low;
  ubyte base_mid;
  ubyte type;
  ubyte limit_flags;
  ubyte base_high;
  uint base_upper;
  uint reserved;
} __attribute__((packed)) tss_descriptor_t;

static ulong gdt[8];
static struct gdt_ptr gdtr;
static tss_t g_tss;

extern void gdt_load_flush(ulong gdtr_addr, ushort data_selector);
extern void tss_load(ushort selector);

static void gdt_set_tss(int index, tss_t *tss) {
  ulong base = (ulong)tss;
  uint limit = sizeof(tss_t) - 1;

  tss_descriptor_t *desc = (tss_descriptor_t *)&gdt[index];
  desc->limit_low = limit & 0xFFFF;
  desc->base_low = base & 0xFFFF;
  desc->base_mid = (base >> 16) & 0xFF;
  desc->type = 0x89; // present, 64-bit TSS available
  desc->limit_flags = ((limit >> 16) & 0xF);
  desc->base_high = (base >> 24) & 0xFF;
  desc->base_upper = (base >> 32) & 0xFFFFFFFF;
  desc->reserved = 0;
}

void gdt_set_kernel_stack(ulong rsp0) {
  g_tss.rsp0 = rsp0;
}

void gdt_init(void) {
  gdt[0] = 0x0000000000000000ULL; // null
  gdt[1] = 0x00AF9A000000FFFFULL; // kernel code   (0x08)
  gdt[2] = 0x00AF92000000FFFFULL; // kernel data   (0x10)
  gdt[3] = 0x00CFFA000000FFFFULL; // user code 32  (0x18) — sysret placeholder
  gdt[4] = 0x00AFF2000000FFFFULL; // user data     (0x20, sel 0x23)
  gdt[5] = 0x00AFFA000000FFFFULL; // user code 64  (0x28, sel 0x2B)
  gdt_set_tss(6, &g_tss);         //               (0x30)

  gdtr.limit = sizeof(gdt) - 1;
  gdtr.base = (ulong)&gdt[0];

  gdt_load_flush((ulong)&gdtr, 0x10);
  tss_load(TSS_SEL);
}