; auto-generated code, do not edit
bits 16			; 16-bit real mode

section _TEXT class=CODE

extern dosdrv_interrupt_                ; <- watcall

global _dosdrv_header
global _dosdrv_req_ptr

; DOS device driver header (must come FIRST)
_dosdrv_header:
        dw  0xFFFF,0xFFFF               ; next device ptr
        dw  0xA800                      ; bit 15: 1=character device
                                        ; bit 14: 0=does not support IOCTL control strings
                                        ; bit 13: 0=does not support output until busy
                                        ; bit 11: 1=understands open/close
                                        ; bit  6: 0=does not understand 3.2 functions
                                        ; bit 3:0: 0=not NUL, STDIN, STDOUT, CLOCK, etc.
        dw  _dosdrv_strategy            ; offset of strategy routine
        dw  dosdrv_interrupt_           ; offset of interrupt routine
        db  'HELLO   '                  ; device name
        db  0

; var storage
_dosdrv_req_ptr:
        dd  0

; =================== Strategy routine ==========
_dosdrv_strategy:
        mov     [cs:_dosdrv_req_ptr+0],bx       ; BYTE ES:BX+0x00 = length of request header
        mov     [cs:_dosdrv_req_ptr+2],es       ; BYTE ES:BX+0x01 = unit number
        retf                            ; BYTE ES:BX+0x02 = command code
                                        ; WORD ES:BX+0x03 = driver return status word
                                        ;      ES:BX+0x05 = reserved??

; EXE entry point points here, in case the user foolishly tries to run this EXE
..start:
_exe_normal_entry:
        mov         ax,4C00h        ; if the user tries to run this EXE, terminate
        int         21h

; Watcom's _END symbol acts as expected in C when you access it, but then acts funny
; when you try to take the address of it. So we have our own.
section _END class=END
global _dosdrv_end
_dosdrv_end:

; begin of init section i.e. the cutoff point once initialization is finished.
section _BEGIN class=INITBEGIN
global _dosdrv_initbegin
_dosdrv_initbegin:

