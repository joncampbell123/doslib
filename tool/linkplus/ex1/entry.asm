
; code
segment _TEXT align=1 class=CODE use16

..start:
global asm_entry
asm_entry:
%ifdef __TINY__
; DOS loads with CS == DS
%else
    mov     ax,_DATA
    mov     ds,ax
%endif

global _printloop
_printloop:
    cld
    mov     si,_msg
l1: lodsb
    or      al,al
    jz      l1e
    mov     dl,al
    mov     ah,2
    int     21h
    jmp     short l1
l1e:

global _exit_
_exit_:
    mov     ah,4Ch
    int     21h

; data
segment _DATA align=1 class=DATA use16

global _msg
_msg:
    db      'Hello world',13,10,0

