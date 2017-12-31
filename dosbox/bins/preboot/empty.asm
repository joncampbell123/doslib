bits 16			; 16-bit real mode
org 0		    ; blob in ROM

; This binary blob should be loaded by DOSBox-X into the ROM BIOS area and
; executed just before the BIOS boots.
;
; Our code and data is mapped read-only in ROM.
;
; This code is expected to return to the BIOS by IRET.
; There is a stack.

    nop
    nop
    nop
    iret

