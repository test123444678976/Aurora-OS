/* MBR opcional: permite arrancar a mesma imagem .iso a partir de uma pen USB (dd). */
    .code16
    .text
    .global _start
_start:
    cli
    xorw    %ax, %ax
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x7C00, %sp
    movw    $0x7C00, %si
    movw    $0x0600, %di
    movw    $256, %cx
    cld
    rep movsw
    ljmp    $0x0000, $relocated
relocated:
    sti
    movb    %dl, drive
    movw    $TOTAL, %bp
1:  movw    $32, %cx
    cmpw    %cx, %bp
    jae     2f
    movw    %bp, %cx
2:  movw    %cx, dap_cnt
    movw    $dap, %si
    movb    drive, %dl
    movb    $0x42, %ah
    int     $0x13
    jc      err
    movw    dap_cnt, %cx
    movzwl  %cx, %eax
    addl    %eax, dap_lba
    shlw    $5, %cx
    addw    %cx, dap_seg
    movw    dap_cnt, %cx
    subw    %cx, %bp
    jnz     1b
    movb    drive, %dl
    ljmp    $0x0000, $0x7C00
err:
    movw    $msg, %si
3:  lodsb
    testb   %al, %al
    jz      4f
    movb    $0x0E, %ah
    movw    $7, %bx
    int     $0x10
    jmp     3b
4:  hlt
    jmp     4b
drive:   .byte 0
msg:     .asciz "Erro a ler o disco.\r\n"
    .align 4
dap:     .byte 16, 0
dap_cnt: .word 0
dap_off: .word 0x7C00
dap_seg: .word 0
dap_lba: .long LBA, 0
