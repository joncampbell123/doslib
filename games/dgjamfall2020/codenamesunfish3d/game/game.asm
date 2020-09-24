;-----------------------------------------------------
; Game entry point. Runs parts as needed.
;-----------------------------------------------------
        bits    16          ; 16-bit real mode
        org     0x100       ; .COM starts at 0x100

        segment .text

        ; make sure ES = DS = CS
        mov     ax,cs
        mov     ds,ax
        mov     es,ax

        ; zero bss and stack
        cld
        mov     di,bss_begin
        mov     cx,(ENDOFIMAGE+1-bss_begin)/2
        xor     ax,ax
        rep     stosw

        ; set up stack
        mov     sp,stack_end-2

exit:
        mov     ah,4Ch
        mov     al,[errcode]
        int     21h

        segment .bss

bss_begin:
exec_fcb:   resb    24
exec_pblk:  resb    0x14
errcode:    resb    1

; stack. try to keep WORD aligned
        alignb  2

stack_beg:  resb    0x400

stack_end:
ENDOFIMAGE: resb    1

