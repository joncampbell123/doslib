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

    mov         bp,sp               ; -> make stack ptr accessible
    lea         ax,[bp-S_BASE]
    mov         [bp],ax             ; -> S_BASE = -28

    sub         word [bp-2-S_BASE],3; -> S_BASE =  -2   Adjust IP

    cld                             ; prepare video RAM writing
    mov         ax,VIDSEG
    mov         es,ax
    xor         di,di               ; ES:DI where to write (upper left corner)

    mov         bx,[bp-2-S_BASE]    ; get IP (the base of our data)
    add         bx,string2x

; TODO
    jmp         short $

; strings
string2x:
    db          'SP','SS','ES','DS'
    db          'CS','FL','BP','DI'
    db          'SI','DX','CX','BX'
    db          'AX','IP'

; padding

    times (510-($-$$)) db 0

; sig

    db      0x55,0xAA

