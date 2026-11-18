ENTRY(_start)
SECTIONS
{
    . = 0x7C00;
    .text   : { *(.boot) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data   : { *(.data .data.*) }
    __bss_start = .;
    .bss    : { *(.bss .bss.*) *(COMMON) }
    __bss_end = .;
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
