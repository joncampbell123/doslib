; auto-generated code, do not edit
%ifidni segment_use,use32
bits 32          ; 32-bit real mode
%else
bits 16          ; 16-bit real mode
%endif

; code
%ifidni segment_use,use32
section _TEXT align=1 class=CODE use32
%else
 %ifidni segment_use,use16
segment _TEXT align=1 class=CODE use16
 %else
  %error unknown or undefined segment_use
 %endif
%endif

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

; data
%ifidni segment_use,use32
section _DATA align=1 class=DATA use32
%else
 %ifidni segment_use,use16
segment _DATA align=1 class=DATA use16
 %else
  %error unknown or undefined segment_use
 %endif
%endif

global _hellostr2
_hellostr2:
    db      'Hello world from ASM',13,10,0

; stack
%ifidni segment_use,use32
section _STACK align=4 class=STACK use32
%else
 %ifidni segment_use,use16
segment _STACK align=4 class=STACK use16
 %else
  %error unknown or undefined segment_use
 %endif
%endif

%ifdef TINYMODE
group DGROUP _DATA _TEXT _STACK
%else
 %if TARGET_MSDOS == 32
group DGROUP _DATA _STACK
 %else
group DGROUP _DATA
 %endif
%endif
