OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

KERNEL_PHYS_BASE = 0x00200000;
KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;

SECTIONS
{
    . = KERNEL_PHYS_BASE;

    __bootstrap_phys_start = .;

    .bootstrap.text ALIGN(4K) : AT(ADDR(.bootstrap.text)) {
        KEEP(*(.bootstrap.text))
        KEEP(*(.bootstrap.text.*))
    }

    .bootstrap.rodata ALIGN(4K) : AT(ADDR(.bootstrap.rodata)) {
        *(.bootstrap.rodata)
        *(.bootstrap.rodata.*)
    }

    .bootstrap.data ALIGN(4K) : AT(ADDR(.bootstrap.data)) {
        *(.bootstrap.data)
        *(.bootstrap.data.*)
    }

    .bootstrap.bss ALIGN(4K) : AT(ADDR(.bootstrap.bss)) {
        *(.bootstrap.bss)
        *(.bootstrap.bss.*)
    }

    __bootstrap_phys_end = ALIGN(., 4K);

    . = KERNEL_VIRT_BASE;

    .text ALIGN(4K) : AT(__bootstrap_phys_end) {
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4K) : AT(ALIGN(LOADADDR(.text) + SIZEOF(.text), 4K)) {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4K) : AT(ALIGN(LOADADDR(.rodata) + SIZEOF(.rodata), 4K)) {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4K) : AT(ALIGN(LOADADDR(.data) + SIZEOF(.data), 4K)) {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }
__kernel_virt_start = ADDR(.text);
__kernel_virt_end   = ALIGN(ADDR(.bss) + SIZEOF(.bss), 4K);

__kernel_phys_start = LOADADDR(.text);
__kernel_phys_end   = ALIGN(LOADADDR(.bss) + SIZEOF(.bss), 4K);

    /DISCARD/ : {
        *(.eh_frame)
        *(.note*)
        *(.comment)
    }
}