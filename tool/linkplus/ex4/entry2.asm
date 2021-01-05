
; code
segment _TEXT align=1 class=CODE use16

global _nops
_nops:
        nop
        nop

; data
segment _DATA align=1 class=DATA use16

global _msg2
_msg2:
    db      'Do not print this',13,10,0

segment _STACK align=4 class=STACK use16

    resb    256

