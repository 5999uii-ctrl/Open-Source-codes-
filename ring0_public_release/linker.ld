/* =========================================================================
 * A.A OS - Bare-Metal Kernel Linker Script
 * Places code at 0x8000, exports _bss_start and _bss_end for BSS zeroing.
 * ========================================================================= */

ENTRY(_start)
SECTIONS
{
    . = 0x8000;

    .text : ALIGN(4) {
        *(.text*)
    }

    .rdata : ALIGN(4) {
        *(.rdata*)
        *(.rodata*)
    }

    .data : ALIGN(4) {
        *(.data*)
    }

    .bss : ALIGN(4) {
        _bss_start = .;
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
        *(.note*)
        *(.reloc)
        *(.idata)
        *(.idata*)
    }
}
