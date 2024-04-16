
; Intercept INT 21h and prevent using AH=25 to hook the keyboard ISR.
; Needed for demos that have either buggy keyboard ISRs or corrupt it during the demo (Incentiv).

org         0x100
bits        16

; Entry point
; -----------
            xor     ax,ax
            mov     es,ax
            push    cs
            pop     ds
            cli

; save old INT 21h vector
            mov     ax,word [es:(0x21*4)]
            mov     bx,word [es:(0x21*4+2)]
            mov     word [old_int21],ax
            mov     word [old_int21+2],bx

; write new vector
            mov     word [es:(0x21*4)],new_int21
            mov     word [es:(0x21*4+2)],cs

; terminate and stay resident
            sti
            mov     ax,3100h
            mov     dx,the_end + 0xF
            mov     cl,4
            shr     dx,cl
            int     21h
            jmp     short $

; old INT 21h vector
old_int21   dd      0

; new INT 21h
new_int21:
            cmp     ax,0x2509               ; we're looking for "set interrupt vector" interrupt vector 9 (IRQ1)
            je      new_int21_skip
new_int21_pass:
            jmp far word [cs:old_int21]
new_int21_skip:
            iret

; end symbol
the_end:    db      'No Keyboard ISR interception hook (C) 2024 Hackipedia/DOSLIB project'

