
	PROG segment para public 'CODE'
	assume cs:PROG
	assume ds:PROG

	public	_start
	public	_hello
	public	_hello2

	org	400h

	nop
_start:	mov	ax,4C00h
	int	21h
	ret

	org	0

_hello:
	db	'Hello world'

	org	8000h

_hello2:
	db	'And here'

	PROG ends

	end _start

