
section _TEXT align=1 class=CODE use32

global entry_code32
entry_code32:
    mov     eax,0x12345678
    mov     esi,_ex_data32
    call    function32

function32:
    ret

section _DATA align=1 class=DATA use32

global _ex_data32
_ex_data32:
    dd      0x89ABCDEF
    db      'HELLO',0

