
bits 16          ; 16-bit real mode

segment _TEXT align=1 class=CODE use16

..start:
                mov     ax,4C00h
                int     21h
                int     20h

