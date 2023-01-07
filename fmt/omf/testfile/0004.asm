
; assemble with Netwide Assembler

	bits	16
	segment	text CODE
	global	_global1

	nop
_global1:
	nop
..start:
	ret

	segment stack STACK

stack1:
	resb 1024

