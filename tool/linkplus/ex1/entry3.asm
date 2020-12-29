
%define TINYMODE
%define DOSLIB_LINKER

; The DOSLIB linker can link EXE images as if COM, with a segment base of 0x100 for DGROUP.
; furthermode, MS-DOS appears to execute the entry point with DS == ES == PSP segment which
; is of course 0x100 bytes before the first resident byte of the EXE image.
;
; Therefore in this build, even though compiling to EXE, this version omits the MOV AX,_DATA
; and mov DS,AX initial code.

%include "entry.asm"

