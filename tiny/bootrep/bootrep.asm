bits 16			; 16-bit real mode

org 0		    ; this code is as PIC as possible

; entry point

; stack storage
%define S_BASE  (-28)

; use CALL to store IP (instruction pointer)
start:                              ; -> S_BASE =   0
    call        start2              ; -> S_BASE =  -2
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
    push        ax                  ; -> S_BASE = -28

    mov         bp,sp               ; -> make stack ptr accessible
    mov         [bp],sp             ; -> S_BASE = -28 = SP

; TODO
    jmp         short $

; padding

    times (510-($-$$)) db 0

; sig

    db      0x55,0xAA

