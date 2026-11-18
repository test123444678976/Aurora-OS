/*
 * AuroraOS - bootloader (BIOS, El Torito "sem emulacao")
 * A BIOS carrega TODA esta imagem (bootloader + kernel) para 0000:7C00.
 * Passos: modo VBE linear framebuffer -> modo protegido -> kmain (C).
 */
    .code16
    .section .boot, "awx"
    .global _start
_start:
    ljmp    $0x0000, $start16
start16:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x7C00, %sp
    cld
    sti
    movb    %dl, boot_drive

    movw    $msg_boot, %si
    call    puts16

    movw    $mode_list, %si
.try_mode:
    lodsw
    testw   %ax, %ax
    jz      .no_vbe
    movw    %si, saved_si
    movw    %ax, cur_mode

    /* informacao do modo -> 8000:0000 */
    movw    $0x8000, %ax
    movw    %ax, %es
    xorw    %di, %di
    movw    cur_mode, %cx
    movw    $0x4F01, %ax
    int     $0x10
    cmpw    $0x004F, %ax
    jne     .next_mode
    movw    $0x8000, %ax
    movw    %ax, %es
    movw    %es:0x00, %ax            /* ModeAttributes */
    andw    $0x0081, %ax             /* suportado + framebuffer linear */
    cmpw    $0x0081, %ax
    jne     .next_mode
    movb    %es:0x19, %al            /* bits por pixel */
    cmpb    $32, %al
    je      .bpp_ok
    cmpb    $24, %al
    jne     .next_mode
.bpp_ok:
    cmpb    $6, %es:0x1B             /* modelo "direct colour" */
    jne     .next_mode

    /* activa o modo (bit 14 = framebuffer linear) */
    movw    cur_mode, %bx
    orw     $0x4000, %bx
    movw    $0x4F02, %ax
    int     $0x10
    cmpw    $0x004F, %ax
    jne     .next_mode

    movw    $0x8000, %ax
    movw    %ax, %es
    movl    %es:0x28, %eax
    movl    %eax, vbe_fb
    movw    %es:0x10, %ax
    movw    %ax, vbe_pitch
    movw    %es:0x12, %ax
    movw    %ax, vbe_w
    movw    %es:0x14, %ax
    movw    %ax, vbe_h
    movb    %es:0x19, %al
    movb    %al, vbe_bpp
    xorw    %ax, %ax
    movw    %ax, %es
    jmp     .go_pm

.next_mode:
    xorw    %ax, %ax
    movw    %ax, %es
    movw    saved_si, %si
    jmp     .try_mode

.no_vbe:
    movw    $msg_novbe, %si
    call    puts16
1:  hlt
    jmp     1b

puts16:
    lodsb
    testb   %al, %al
    jz      2f
    movb    $0x0E, %ah
    movw    $0x0007, %bx
    int     $0x10
    jmp     puts16
2:  ret

.go_pm:
    cli
    lgdt    gdt_desc
    inb     $0x92, %al               /* A20 rapido */
    orb     $2, %al
    andb    $0xFE, %al
    outb    %al, $0x92
    movl    %cr0, %eax
    orl     $1, %eax
    movl    %eax, %cr0
    ljmp    $0x08, $pm_entry

    .code32
pm_entry:
    movw    $0x10, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %fs
    movw    %ax, %gs
    movw    %ax, %ss
    movl    $0x90000, %esp
    cld
    call    kmain
1:  cli
    hlt
    jmp     1b

/* ---------- dados (ficam no 1.o sector para serem enderecaveis em 16 bits) ---------- */
    .align 8
gdt:
    .quad   0x0000000000000000
    .quad   0x00CF9A000000FFFF       /* 0x08: codigo 4GB */
    .quad   0x00CF92000000FFFF       /* 0x10: dados  4GB */
gdt_end:
gdt_desc:
    .word   gdt_end - gdt - 1
    .long   gdt

mode_list:
    .word   0x118, 0x144, 0x115, 0x143, 0x112, 0x141, 0x11B, 0

boot_drive: .byte 0
saved_si:   .word 0
cur_mode:   .word 0

    .global vbe_fb, vbe_pitch, vbe_w, vbe_h, vbe_bpp
    .align 4
vbe_fb:     .long 0
vbe_pitch:  .word 0
vbe_w:      .word 0
vbe_h:      .word 0
vbe_bpp:    .byte 0

msg_boot:   .asciz "AuroraOS: a arrancar...\r\n"
msg_novbe:  .asciz "Erro: modo grafico VBE nao suportado.\r\n"
