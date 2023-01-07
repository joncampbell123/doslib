
	PROG segment para public 'CODE'
	assume cs:PROG
	assume ds:PROG

	org	400h

	nop
_start:	mov	ax,4C00h
	int	21h
	ret

	org	0

	db	'Hello world'

	org	8000h

	db	'And here'

	PROG ends

	end _start

