bits 16			; 16-bit real mode

org 0		    ; this code is as PIC as possible

; entry point

; video segment
%ifdef TARGET_PC98
 %define VIDSEG 0xA000
%else
 %define VIDSEG 0xB800
%endif

; stack storage
%define S_BASE  (-28)

; use CALL to store IP (instruction pointer)
start:                              ; -> S_BASE =   0
    call        start2              ; -> S_BASE =  -2    NTS: Stored value is orig IP + 3
start2:
    push        ax                  ; -> S_BASE =  -4
    push        bx                  ; -> S_BASE =  -6
    push        cx                  ; -> S_BASE =  -8
    push        dx                  ; -> S_BASE = -10
    push        si                  ; -> S_BASE = -12
    push        di                  ; -> S_BASE = -14
    push        bp                  ; -> S_BASE = -16
    pushf                           ; -> S_BASE = -18
    push        cs                  ; -> S_BASE = -20
    push        ds                  ; -> S_BASE = -22
    push        es                  ; -> S_BASE = -24
    push        ss                  ; -> S_BASE = -26
    push        ax                  ; -> S_BASE = -28   will become SP

    cli                             ; turn off interrupts

    mov         bp,sp               ; -> make stack ptr accessible
    lea         ax,[bp-S_BASE]
    mov         [bp],ax             ; -> S_BASE = -28

    sub         word [bp-2-S_BASE],3; -> S_BASE =  -2   Adjust IP

    cld                             ; prepare video RAM writing
    mov         ax,VIDSEG
    mov         es,ax
    xor         di,di               ; ES:DI where to write (upper left corner)

    push        cs
    pop         ds

    mov         bx,[bp-2-S_BASE]    ; get IP (the base of our data)
    lea         si,[bx+hexes]       ; SI = hex strings
    add         bx,string2x

    mov         cx,2

ploopo:
    push        cx
    push        di
    mov         cx,(-S_BASE) / 2 / 2

; -----------------------------------
; So:
;   SS:BP = SP + S_BASE = start of struct
;   ES:DI = video RAM
;   DS:BX = 2-char strings
;   DS:SI = &hexes[0]
;   CX    = count
; -----------------------------------
ploop:
    mov         ax,[bx]
    add         bx,2
    call        putc
    mov         al,ah
    call        putc

    mov         al,' '
    call        putc

    mov         ax,[bp]
    add         bp,2
    call        puthex16

    mov         al,' '
    call        putc

    loop        ploop
;-------------------------------------
    pop         di
    pop         cx
    add         di,80*2
    loop        ploopo

; STOP
    jmp         short $

; putc
; in:
; AL = char
; ES:DI = VRAM
; out:
; AL = destroyed
; DI += 2
putc:
%ifdef TARGET_PC98
    push        ax
    xor         ah,ah           ; keep upper byte zero, single wide
    stosw
    pop         ax
    mov         al,0xA1         ; cyan, not invisible
    es
    mov         [di+0x2000-2],al; ES:DI = attribute   (STOSW already added 2 to DI)
%else
    push        ax
    mov         ah,0x0B         ; VGA cyan
    stosw
    pop         ax
%endif
    ret

; puthex16
; in:
; AX = 16-bit value
; ES:DI = VRAM
; out:
; AX = destroyed
; DI += 2
puthex16:
    push        cx
    mov         cl,4

    rol         ax,cl
    call        puthex4

    rol         ax,cl
    call        puthex4

    rol         ax,cl
    call        puthex4

    rol         ax,cl
    call        puthex4

    pop         cx
    ret

; puthex4
; in:
; AL = lower 4 bits are digit to print
; ES:DI = VRAM
; DS:SI = hexes
; out:
; AL = destroyed
puthex4:
    push        ax
    push        bx
    mov         bl,al
    and         bl,0xF
    xor         bh,bh
    mov         al,[bx+si]
    pop         bx
    call        putc
    pop         ax
    ret

; strings
string2x:
    db          'SP','SS','ES','DS'
    db          'CS','FL','BP','DI'
    db          'SI','DX','CX','BX'
    db          'AX','IP'

; hexdump
hexes:
    db          '01234567'
    db          '89ABCDEF'

; padding

    times (510-($-$$)) db 0

; sig

    db      0x55,0xAA

