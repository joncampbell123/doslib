; auto-generated code, do not edit
bits 16          ; 16-bit real mode

%include '_memmodl.inc'

%include "_watcall.inc"

%include "_comregn.inc"

%include "_segcode.inc"

..start:
global asm_entry
asm_entry:

extern entry_c_

%ifndef TINYMODE
    mov     ax,_DATA
    mov     ds,ax
%else
    ; tiny model (.com) already has CS == DS
%endif

%ifdef MMODE_CODE_CALL_DEF_FAR
    call far entry_c_
%else
    call    entry_c_
%endif

global _exit_
_exit_:
    mov     ah,4Ch
    int     21h

%include "_segdata.inc"

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

group DGROUP _DATA

