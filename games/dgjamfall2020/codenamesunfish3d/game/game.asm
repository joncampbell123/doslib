;-----------------------------------------------------
; Game entry point. Runs parts as needed.
;-----------------------------------------------------
        bits    16          ; 16-bit real mode
        org     0x100       ; .COM starts at 0x100

; highest error code that means something
%define HIGHEST_CODE        0x43

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

        ; run game. each error exit state says what to do next.
        mov     di,exe_parto1
        mov     si,ct_init_run
execagain:
        call    execit

        mov     ax,[errcode]
        or      ah,ah                               ; if the termination is anything other than normal, exit (see INT 21h AH=4Dh)
        jnz     exit                                ; jump if AH != 0

        cmp     al,40h                              ; error codes less than 40h mean exit to DOS
        jb      exit
        cmp     al,HIGHEST_CODE
        ja      exit
        sub     al,40h
        mov     bx,ax
        add     bx,ax                               ; so BX = (AX - 40h) * 2
        jmp     word [errcodehandlertable+bx]

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
        mov     word [errcode],sp               ; stash the stack pointer in errcode so we can recover it after DOS returns from EXEC

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
        mov     sp,[errcode]                    ; restore SP
        sti

; get the return code. note that reading it clears it, so save it away.
; AH = termination type   0=normal  1=CTRL+C  2=critical error  3=TSR
        mov     ah,4Dh
        int     21h
        mov     word [errcode],ax

; return
        ret

; error code handler table (offset of functions)
errcodehandlertable:
        dw      errh_40h
        dw      errh_41h
        dw      errh_42h
        dw      errh_43h

; ERR=40h not yet assigned, dump to DOS
errh_40h:
        jmp     exit

; ERR=41h minigame 1
errh_41h:
        mov     di,exe_parto1
        mov     si,ct_after_minigame
        jmp     execagain

; ERR=42h minigame 2
errh_42h:
        mov     di,exe_parto1
        mov     si,ct_after_minigame
        jmp     execagain

; ERR=43h minigame 3
errh_43h:
        mov     di,exe_parto1
        mov     si,ct_after_minigame
        jmp     execagain

; error message, in MS-DOS friendly ASCII$ format meaning $-terminated
execiterr:db    'EXEC error',13,10,'$'

; what to run and how.
; note a "command tail" in DOS means literally the data copied to PSP:0x80.
; That includes number of bytes too.
exe_parto1:db   'PARTO1.EXE',0
ct_init_run:db  0                       ; nothing
ct_after_minigame:db    5,'MGRET',0     ; return from minigame

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

