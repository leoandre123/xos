#include "boot_info.h"
#include "types.h"

#define BOOT_CODE   __attribute__((section(".bootstrap.text")))
#define BOOT_DATA   __attribute__((section(".bootstrap.bss"), aligned(4096)))
#define BOOT_RODATA __attribute__((section(".bootstrap.rodata")))

#define PAGE_PRESENT   (1ULL << 0)
#define PAGE_WRITABLE  (1ULL << 1)
#define PAGE_HUGE      (1ULL << 7)
#define PAGE_SIZE      0x1000ULL
#define PAGE_SIZE_2M   0x200000ULL
#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000ULL

// Must match the value used by the rest of your kernel/VMM.
#define HHDM_BASE 0xFFFF800000000000ULL

typedef ulong page_table_t[512];

static const char STR_BOOTSTRAP[] BOOT_RODATA = "\r\nBOOTSTRAP\r\n";
static const char STR_PHYS_START[] BOOT_RODATA = "PHYS START: ";
static const char STR_PHYS_END[] BOOT_RODATA = "PHYS END: ";
static const char STR_VIRT_START[] BOOT_RODATA = "VIRT START: ";
static const char STR_VIRT_END[] BOOT_RODATA = "VIRT END: ";
static const char STR_ENTRY[] BOOT_RODATA = "ENTRY: ";
static const char STR_PAGE_COUNT[] BOOT_RODATA = "PAGE COUNT: ";
static const char STR_CR3[] BOOT_RODATA = "CR3 LOADED!\r\n";
static const char STR_NEWLINE[] BOOT_RODATA = "\r\n";

extern char __kernel_phys_start[];
extern char __kernel_phys_end[];
extern char __kernel_virt_start[];
extern char __kernel_virt_end[];

extern void enter_higher_half(BootInfo *boot_info);
extern ulong get_kernel_main_addr(void);

BOOT_DATA static page_table_t g_pml4;

BOOT_DATA static page_table_t g_low_pdpt;
BOOT_DATA static page_table_t g_low_pd;

BOOT_DATA static page_table_t g_high_pdpt;
BOOT_DATA static page_table_t g_high_pd;
BOOT_DATA static page_table_t g_high_pt;

// New: bootstrap HHDM tables
BOOT_DATA static page_table_t g_hhdm_pdpt;
BOOT_DATA static page_table_t g_hhdm_pd;

BOOT_CODE static void memset64(ulong *ptr, ulong value, ulong count) {
  for (ulong i = 0; i < count; ++i)
    ptr[i] = value;
}

BOOT_CODE static inline ulong pml4_index(ulong a) { return (a >> 39) & 0x1FF; }
BOOT_CODE static inline ulong pdpt_index(ulong a) { return (a >> 30) & 0x1FF; }
BOOT_CODE static inline ulong pd_index(ulong a) { return (a >> 21) & 0x1FF; }

BOOT_CODE static inline void write_cr3(ulong value) {
  __asm__ volatile("mov %0, %%cr3" : : "r"(value) : "memory");
}

BOOT_CODE static inline void outb_boot(ushort port, ubyte value) {
  asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

BOOT_CODE static inline ubyte inb_boot(ushort port) {
  ubyte ret;
  asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

BOOT_CODE void boot_serial_write_char(char c) {
  while ((inb_boot(0x3F8 + 5) & 0x20) == 0) {
  }
  outb_boot(0x3F8, (ubyte)c);
}

BOOT_CODE void serial_write_boot(const char *s) {
  while (*s)
    boot_serial_write_char(*s++);
}

BOOT_CODE void boot_serial_write_hex64(ulong value) {
  for (int i = 15; i >= 0; --i) {
    ubyte nibble = (value >> (i * 4)) & 0xF;
    boot_serial_write_char(nibble < 10 ? ('0' + nibble) : ('a' + nibble - 10));
  }
}

BOOT_CODE void bootstrap_main(BootInfo *boot_info) {
  memset64((ulong *)g_pml4, 0, 512);

  memset64((ulong *)g_low_pdpt, 0, 512);
  memset64((ulong *)g_low_pd, 0, 512);

  memset64((ulong *)g_high_pdpt, 0, 512);
  memset64((ulong *)g_high_pd, 0, 512);
  memset64((ulong *)g_high_pt, 0, 512);

  memset64((ulong *)g_hhdm_pdpt, 0, 512);
  memset64((ulong *)g_hhdm_pd, 0, 512);

  serial_write_boot(STR_BOOTSTRAP);

  // ------------------------------------------------------------
  // 1. Low identity map: first 1 GiB using 2 MiB huge pages
  // ------------------------------------------------------------
  g_pml4[0] =
      ((ulong)g_low_pdpt & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;
  g_low_pdpt[0] =
      ((ulong)g_low_pd & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;

  for (ulong i = 0; i < 512; ++i) {
    g_low_pd[i] =
        (i * PAGE_SIZE_2M) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
  }

  // ------------------------------------------------------------
  // 2. Higher-half kernel map
  // ------------------------------------------------------------
  ulong phys_start = (ulong)__kernel_phys_start;
  ulong phys_end = (ulong)__kernel_phys_end;
  ulong virt_start = (ulong)__kernel_virt_start;
  ulong virt_end = (ulong)__kernel_virt_end;

  // round up, not down
  ulong page_count = (phys_end - phys_start + PAGE_SIZE - 1) / PAGE_SIZE;

  ulong pml4i = pml4_index(virt_start);
  ulong pdpti = pdpt_index(virt_start);
  ulong pdi = pd_index(virt_start);

  g_pml4[pml4i] =
      ((ulong)g_high_pdpt & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;
  g_high_pdpt[pdpti] =
      ((ulong)g_high_pd & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;
  g_high_pd[pdi] =
      ((ulong)g_high_pt & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;

  for (ulong i = 0; i < page_count; ++i) {
    g_high_pt[i] =
        (phys_start + i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITABLE;
  }

  // ------------------------------------------------------------
  // 3. HHDM: first 1 GiB direct-mapped using 2 MiB huge pages
  //    virt = HHDM_BASE + phys
  // ------------------------------------------------------------
  ulong hhdm_pml4i = pml4_index(HHDM_BASE);
  ulong hhdm_pdpti = pdpt_index(HHDM_BASE);

  g_pml4[hhdm_pml4i] =
      ((ulong)g_hhdm_pdpt & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;
  g_hhdm_pdpt[hhdm_pdpti] =
      ((ulong)g_hhdm_pd & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE;

  for (ulong i = 0; i < 512; ++i) {
    g_hhdm_pd[i] =
        (i * PAGE_SIZE_2M) | PAGE_PRESENT | PAGE_WRITABLE | PAGE_HUGE;
  }

  serial_write_boot(STR_PHYS_START);
  boot_serial_write_hex64(phys_start);
  serial_write_boot(STR_NEWLINE);

  serial_write_boot(STR_PHYS_END);
  boot_serial_write_hex64(phys_end);
  serial_write_boot(STR_NEWLINE);

  serial_write_boot(STR_VIRT_START);
  boot_serial_write_hex64(virt_start);
  serial_write_boot(STR_NEWLINE);

  serial_write_boot(STR_VIRT_END);
  boot_serial_write_hex64(virt_end);
  serial_write_boot(STR_NEWLINE);

  serial_write_boot(STR_ENTRY);
  boot_serial_write_hex64(get_kernel_main_addr());
  serial_write_boot(STR_NEWLINE);

  serial_write_boot(STR_PAGE_COUNT);
  boot_serial_write_hex64(page_count);
  serial_write_boot(STR_NEWLINE);

  write_cr3((ulong)g_pml4);
  serial_write_boot(STR_CR3);

  enter_higher_half(boot_info);

  for (;;)
    asm volatile("cli; hlt");
}