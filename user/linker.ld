ENTRY(_start)

SECTIONS {
    . = 0x400000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .init_array : {
        __init_array_start = .;
        *(.init_array*)
        __init_array_end = .;
    }

    .bss : {
        *(.bss*)
        *(COMMON)
    }
}
