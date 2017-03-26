
    bits    16
    org     7C00h

reloc:
    cli
    jmp     0000h:start
start:
    xor     bp,bp
    mov     ds,bp
    mov     es,bp
    mov     ss,bp
    mov     sp,7BFCh
    mov     [ireg_ax],ax
    mov     [ireg_bx],bx
    mov     [ireg_cx],cx
    mov     [ireg_dx],dx
    mov     [ireg_si],si
    mov     [ireg_di],di

;--------------------------------------------------
    mov     si,regs_str
    call    puts

    mov     si,ireg_ax
    mov     cx,6
printregloop:
    lodsw                   ; mov ax,[si]; add si,2
    call    puthex16
    call    putspc
    dec     cx
    jnz     printregloop

    mov     si,crlf
    call    puts

;--------------------------------------------------
    mov     ah,0x08
    mov     dl,[bios_drive]
    xor     di,di
    mov     es,di
    int     13h
    mov     si,cannot_query_drive
    jnc     query_drive_ok
    jmp     err_str
query_drive_ok:

;--------------------------------------------------
    mov     si,query_str
    call    puts

    mov     al,dl               ; number of drives
    call    puthex
    call    putspc

    mov     al,dh               ; number of heads - 1
    inc     al
    call    puthex
    call    putspc

    push    cx
    mov     ax,cx               ; number of cyls (upper two bits [9:8] in CL[7:6] and lower 8 in CH)
    xchg    al,ah
    mov     cl,6
    shr     ah,cl               ; AH = bits [9:8] as [1:0] AL = bits [7:0]
    inc     ax
    call    puthex16
    call    putspc
    pop     cx

    mov     al,cl
    and     al,0x3F
    call    puthex

    mov     si,crlf
    call    puts

;--------------------------------------------------
    mov     si,ok_str
    jmp     err_str

;--------------------------------------------------
ireg_ax:    dw      0
ireg_bx:    dw      0
ireg_cx:    dw      0
ireg_dx:    dw      0
ireg_si:    dw      0
ireg_di:    dw      0

bios_drive: db      80h

cannot_query_drive:
            db      'Query failed',0
ok_str:
            db      'OK',0
regs_str:
            db      'AX/BX/CX/DX/SI/DI ',0
query_str:
            db      'drives/heads/cyls/sects ',0
crlf:
            db      13,10,0
hexes:
            db      '01234567'
            db      '89ABCDEF'

err_str:
    call    puts
    mov     si,crlf
    call    puts
    jmp     short $

puts:
    clc
    push    ax
    push    bx
    push    si
putsl:
    lodsb
    or      al,al
    jz      putse
    mov     ah,0x0E
    xor     bx,bx
    int     10h
    jmp     short putsl
putse:
    pop     si
    pop     bx
    pop     ax
    ret

putc:
    push    ax
    push    bx
    xor     bx,bx
    mov     ah,0x0E
    int     10h
    pop     bx
    pop     ax
    ret

putspc:
    mov     al,' '
    call    putc
    ret

puthex: ; print AL
    push    ax
    push    bx
    push    cx
    mov     cx,2

puthexl:
    push    cx
    mov     cl,4
    rol     al,cl

    push    ax
    mov     bx,hexes
    and     al,0x0F
    xlat
    xor     bx,bx
    mov     ah,0x0E
    int     10h
    pop     ax
    pop     cx

    dec     cx
    jnz     puthexl

    pop     cx
    pop     bx
    pop     ax
    ret

puthex16: ; print AX
    push    ax

    xchg    al,ah
    call    puthex

    xchg    al,ah
    call    puthex

    pop     ax
    ret

