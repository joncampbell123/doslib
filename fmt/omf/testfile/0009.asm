
; assemble with Netwide Assembler

	segment	CODE class=CODE align=16 USE16
	global	_global1
	global	x1,x2

	common	pingas 32
	common	mushroom_kingdom 24
	common	nearsymbol 16:near
	common	farsymbol 16:far

	nop
_global1:
	nop
..start:
	ret

	bits	32
	segment	CODE2 class=CODE align=16 USE32
	global	_global2
	global	x1,x2

	mov	eax,ebx
_global2:
	mov	esi,0x12345678
	ret

	common	big_chungus 4096:near
	common	fbig_chungus 4096:far

	common	pingas 32
	common	mushroom_kingdom 24
	common	nearsymbol 16:near
	common	farsymbol 16:far

	segment SCREEN ABSOLUTE=0xB800

vimem:
	resw 80*25

	segment BSS class=BSS align=16 nobits

x1:	resw 1
x2:	resd 1

	segment stack STACK class=STACK align=4

stack1:
	resb 1024

