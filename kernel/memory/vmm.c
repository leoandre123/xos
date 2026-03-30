#include "vmm.h"
#include "boot_info.h"
#include "io/serial.h"
#include "memory/heap.h"
#include "memory/pmm.h"
#include "panic.h"
#include "types.h"

#define SIZE_TO_PAGE_COUNT(size) (size + PAGE_SIZE - 1) / PAGE_SIZE;

address_space g_kernel_address_space;

static void memset64(ulong *ptr, ulong value, ulong count) {
  for (ulong i = 0; i < count; i++) {
    ptr[i] = value;
  }
}

static page_table *alloc_table() {
  ulong phys = pmm_alloc_page();
  if (!phys)
    panic("Failed to allocate page table");

  page_table *table = (page_table *)PHYS_TO_HHDM(phys);
  memset64((ulong *)table, 0, 512);
  return table;
}

static page_table *get_or_create(page_table *table, ulong index, ulong flags) {
  ulong entry = (table->entries[index]);

  if (entry & PAGE_PRESENT) {
    return (page_table *)PHYS_TO_HHDM(entry & PAGE_ADDR_MASK);
  }

  page_table *next = alloc_table();

  table->entries[index] =
      (HHDM_TO_PHYS((ulong)next) & PAGE_ADDR_MASK) | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);

  return next;
}

static page_table *get_table(page_table *table, ulong index) {
  ulong entry = (table->entries[index]);

  if (entry & PAGE_PRESENT) {
    return (page_table *)PHYS_TO_HHDM(entry & PAGE_ADDR_MASK);
  }
  return 0;
}
static void vmm_unmap_and_free_pages(address_space *space, ulong virtual_addr, ulong page_count) {
  for (ulong i = 0; i < page_count; i++) {
    ulong virt = virtual_addr + i * PAGE_SIZE;

    ulong pml4_index = (virt >> 39) & 0x1FF;
    ulong pdpt_index = (virt >> 30) & 0x1FF;
    ulong pd_index = (virt >> 21) & 0x1FF;
    ulong pt_index = (virt >> 12) & 0x1FF;

    page_table *pml4 = space->pml4;
    page_table *pdpt = get_table(pml4, pml4_index);
    if (!pdpt)
      return;
    page_table *pd = get_table(pdpt, pdpt_index);
    if (!pd)
      return;
    page_table *pt = get_table(pd, pd_index);
    if (!pt)
      return;
    pmm_free_page(pt->entries[pt_index] & PAGE_ADDR_MASK);
    pt->entries[pt_index] = 0;
  }
}

void vmm_map_pages(address_space *space, ulong virtual_addr,
                   ulong phys_addr, ulong page_count, ulong flags) {
  for (ulong i = 0; i < page_count; i++) {
    ulong virt = virtual_addr + i * PAGE_SIZE;
    ulong phys = phys_addr + i * PAGE_SIZE;

    ulong pml4_index = (virt >> 39) & 0x1FF;
    ulong pdpt_index = (virt >> 30) & 0x1FF;
    ulong pd_index = (virt >> 21) & 0x1FF;
    ulong pt_index = (virt >> 12) & 0x1FF;

    page_table *pml4 = space->pml4;
    page_table *pdpt = get_or_create(pml4, pml4_index, flags);
    page_table *pd = get_or_create(pdpt, pdpt_index, flags);
    page_table *pt = get_or_create(pd, pd_index, flags);
    pt->entries[pt_index] = (phys & PAGE_ADDR_MASK) | flags | PAGE_PRESENT;
  }
}

void vmm_map_bytes(address_space *space, ulong virtual_addr,
                   ulong phys_addr, ulong size, ulong flags) {
  ulong page_count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
  vmm_map_pages(space, virtual_addr, phys_addr, page_count, flags);
}

static void vmm_mount_address_space(address_space *space) {
  asm volatile("mov %0, %%cr3" : : "r"(space->pml4_phys) : "memory");
}

static void vmm_map_kernel_image(address_space *space) {
  vmm_map_bytes(space, KERNEL_BASE, (ulong)__kernel_phys_start,
                (ulong)__kernel_phys_end - (ulong)__kernel_phys_start,
                PAGE_PRESENT | PAGE_WRITABLE);
}
static void vmm_map_low_identity(address_space *space) {
  vmm_map_bytes(space, 0x00, 0x00, 4000000, PAGE_PRESENT | PAGE_WRITABLE);
}

static void vmm_map_framebuffer(address_space *space, BootInfo *boot_info) {
  ulong fb_size = boot_info->framebuffer_pitch * boot_info->framebuffer_height;
  vmm_map_bytes(space, FB_BASE, boot_info->framebuffer_base, fb_size, PAGE_WRITABLE);
}

static void vmm_map_hhdm(address_space *space) {
  ulong max_address = pmm_get_max_address();
  vmm_map_bytes(space, HHDM_BASE, 0, max_address, PAGE_WRITABLE);
}
/*
  | API |
*/
address_space *vmm_create_address_space(void) {
  address_space *space = kmalloc(sizeof(address_space));
  if (!space)
    return 0;
  space->pml4_phys = pmm_alloc_page();
  space->pml4 = (page_table *)PHYS_TO_HHDM(space->pml4_phys);
  memset64((ulong *)space->pml4, 0, 512);

  for (int i = 256; i < 512; i++) {
    space->pml4->entries[i] = g_kernel_address_space.pml4->entries[i];
  }

  return space;
}
void vmm_destroy_address_space(address_space *space) {
  panic("vmm_destroy_address_space is not implemented!");
}
void vmm_switch_address_space(address_space *space) {
  vmm_mount_address_space(space);
}

void vmm_init(BootInfo *boot_info) {

  serial_write_line("Initializing vmm...");
  g_kernel_address_space.pml4_phys = pmm_alloc_page();
  if (!g_kernel_address_space.pml4_phys) {
    panic("Cannot allocate pml4 for kernel address space");
  }

  serial_write("pml4_phys: ");
  serial_write_hex(g_kernel_address_space.pml4_phys);
  serial_write("\n");

  g_kernel_address_space.pml4 = (page_table *)PHYS_TO_HHDM(g_kernel_address_space.pml4_phys);
  memset64((ulong *)g_kernel_address_space.pml4, 0, 512);

  serial_write("Mapping kernel image...");
  vmm_map_kernel_image(&g_kernel_address_space);
  serial_write_line("Done!");
  serial_write("Mapping framebuffer...");
  vmm_map_framebuffer(&g_kernel_address_space, boot_info);
  serial_write_line("Done!");
  serial_write("Mapping HHDM...");
  vmm_map_hhdm(&g_kernel_address_space);
  serial_write_line("Done!");
  serial_write("Mapping low identity...");
  vmm_map_low_identity(&g_kernel_address_space);
  serial_write_line("Done!");

  ulong rsp;
  asm volatile("mov %%rsp, %0" : "=r"(rsp));

  serial_write("RSP: ");
  serial_write_hex(rsp);
  serial_write_char('\n');

  vmm_setup_stack();

  serial_write("Mounting kernel address space...");
  vmm_mount_address_space(&g_kernel_address_space);
  serial_write_line("Done!");
}

ulong vmm_setup_stack() {
  serial_write_line("Setting up new stack...");
  ulong phys_stack_addr = pmm_alloc_pages(KERNEL_STACK_PAGE_COUNT);

  if (!phys_stack_addr) {
    panic("Cannot allocate stack pages!");
  }

  ulong virt_stack_base =
      KERNEL_STACK_TOP - KERNEL_STACK_PAGE_COUNT * PAGE_SIZE;

  vmm_map_pages(&g_kernel_address_space, virt_stack_base, phys_stack_addr,
                KERNEL_STACK_PAGE_COUNT, PAGE_PRESENT | PAGE_WRITABLE);

  return KERNEL_STACK_TOP;
}

void vmm_kernel_heap_alloc(ulong offset, ulong size) {
  ulong page_count = SIZE_TO_PAGE_COUNT(size);
  ulong phys_addr = pmm_alloc_pages(page_count);
  vmm_map_pages(&g_kernel_address_space, KERNEL_HEAP + offset, phys_addr, page_count, PAGE_WRITABLE);
}
void vmm_kernel_heap_free(ulong offset, ulong size) {
  ulong page_count = SIZE_TO_PAGE_COUNT(size);
  vmm_unmap_and_free_pages(&g_kernel_address_space, KERNEL_HEAP + offset, page_count);
}

// address_space *vmm_get_kernel_space() { return &g_kernel_address_space; }