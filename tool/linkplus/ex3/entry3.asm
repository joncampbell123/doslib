
; code
segment _TEXT align=4 class=CODE use16

global _nops2
_nops2:
        nop
        nop

; data
segment _DATA align=8 class=DATA use16

global _msg3
_msg3:
    db      'Do not print this either',13,10,0

segment _STACK align=4 class=STACK use16

    resb    64

