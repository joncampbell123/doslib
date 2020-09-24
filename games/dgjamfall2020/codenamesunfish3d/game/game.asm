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

        ; COM executables (and most EXEs specified by the header) by default
        ; take all available memory when executed. To run other programs we
        ; need to shrink our allocated region.
        mov     cl,4                                ; right shift 4 to divide by 16 (2^4 = 2*2*2*2 = 4*4 = 16)
        mov     ah,4Ah                              ; resize memory block specified in ES (which is already CS)
        mov     bx,ENDOFIMAGE+0xF                   ; BX = number of paragraphs. Must include our code, data, bss and PSP segment.
                                                    ; I would *love* to just write (ENDOFIMAGE+15)/16 but NASM won't let me.
        shr     bx,cl
        int     21h

        ; set up stack
        mov     sp,stack_end-2

        ; run game
        mov     di,exe_parto1
        mov     si,ct_init_run
        call    execit

exit:
        mov     ah,4Ch
        mov     al,byte [errcode]
        int     21h

; execit
;  in:
;   SI = command tail
;   DI = EXE to run
;  out:
;   (all registers trashed and SS:SP reset to init state because DOS EXEC)
;   (exit code in [errcode])
;   ES = DS = CS
execit:
        mov     ax,cs
        mov     ds,ax
        mov     es,ax

        mov     word [exec_pblk+0],0            ; copy our own environ
        mov     word [exec_pblk+2],si           ; command tail
        mov     word [exec_pblk+4],cs           ; (segment of)
        mov     word [exec_pblk+6],exec_fcb     ; FCB
        mov     word [exec_pblk+8],cs           ; (segment of)
        mov     word [exec_pblk+10],exec_fcb    ; FCB
        mov     word [exec_pblk+12],cs          ; (segment of)
        mov     word [exec_pblk+14],0

        mov     ax,4B00h                        ; INT 21h AH=4Bh AL=00h load and execute
        mov     dx,di                           ; EXE to run (DS:DX)
        mov     bx,exec_pblk                    ; param block (ES:BX)
        int     21h                             ; GO!
        jnc     execitok
; error on exec
        mov     ah,9                            ; write string to output
        mov     dx,execiterr
        int     21h
        mov     word [errcode],0xFFFF
        ret

; it ran, read return code. unfortunately DOS trashes all CPU registers and we need to cleanup.

execitok:
        mov     ax,cs
        mov     ds,ax
        mov     es,ax
        cli
        mov     ss,ax
        mov     sp,stack_end-2
        sti

; get the return code. note that reading it clears it, so save it away.
; AH = termination type   0=normal  1=CTRL+C  2=critical error  3=TSR
        mov     ah,4Dh
        int     21h
        mov     word [errcode],ax

; return
        ret

; error message, in MS-DOS friendly ASCII$ format meaning $-terminated
execiterr:db    'EXEC error',13,10,'$'

; what to run and how
exe_parto1:db   'PARTO1.EXE',0
ct_init_run:db  0                   ; nothing

; END OF CODE/DATA
        segment .bss

bss_begin:
exec_fcb:   resb    24
exec_pblk:  resb    0x14
; [http://www.ctyme.com/intr/rb-2939.htm]
;  +0x00 WORD       segment of environment block to copy for child (or 0 to include our own)
;  +0x02 DWORD      pointer to command tail (holds argv as one big string)
;  +0x06 DWORD      pointer to first FCB (we just point at zeros, works fine)
;  +0x0A DWORD      pointer to second FCB (same thing)
;  (other stuff not relevant to how we use EXEC)
errcode:    resw    1

; stack. try to keep WORD aligned
            alignb  2

stack_beg:  resb    0x400

stack_end:
ENDOFIMAGE: resb    1

