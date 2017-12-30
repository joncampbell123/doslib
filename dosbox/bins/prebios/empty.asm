bits 16			; 16-bit real mode
org 0		    ; blob in ROM

; This binary blob should be loaded by DOSBox-X into the ROM BIOS area and
; executed just before the POST routine. But only once. The BIOS will not
; run us again until DOSBox-X exits.
;
; Our code and data is mapped read-only in ROM.

    nop
    nop
    nop
    iret        ; IRETF to the BIOS

