
segment _TEXT align=1 class=CODE use16

..start:
global asm_entry
asm_entry:

extern entry_c_

%ifndef TINYMODE
    mov     ax,_DATA
    mov     ds,ax
%else
    ; the DOSLIB linker can properly link .com executables
%endif

; unsigned int near entry_c(void)
    call    entry_c_

global _exit_
_exit_:
    mov     ah,4Ch
    int     21h

segment _DATA align=1 class=DATA use16

segment _STACK align=1 class=STACK use16

