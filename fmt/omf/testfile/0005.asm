
; assemble with Netwide Assembler

	bits	16
	segment	CODE class=CODE
	global	_global1
	global	x1,x2

	nop
_global1:
	nop
..start:
	ret

	segment BSS class=BSS

x1:	resw 1
x2:	resd 1

	segment stack STACK class=STACK

stack1:
	resb 1024

