
; data
segment _DATA align=1 class=DATA use16

global _msg
_msg:
    db      'Hello world',13,10,0

; code
segment _TEXT align=1 class=CODE use16

    db      'Blah blah blah blah blah blah blah blah blah padding the entry point out past 128 bytes blah blah blah blah blah blah blah blah blah blah blah blah blah blah',13,10,0

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
    ret

..start:
global asm_entry
asm_entry:
; even for .COM, load segment (induce segment relocation)
    mov     ax,_DATA
    mov     ds,ax

    call    _printloop

global _exit_
_exit_:
    mov     ah,4Ch
    int     21h

segment _STACK align=4 class=STACK use16

    resb    512

section _BSS align=4 class=BSS use16

    resb    64

%ifdef TINYMODE
group DGROUP _DATA _TEXT _STACK _BSS
%else
 %if TARGET_MSDOS == 32
group DGROUP _DATA _STACK
 %else
group DGROUP _DATA
 %endif
%endif
