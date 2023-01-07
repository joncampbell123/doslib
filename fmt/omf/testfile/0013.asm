
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

	; iterated data
	dw	512 dup(1234h)

	db	'Normal data'

	; iterated data #2
	dw	32 dup(1234h,0ABCDh)

	db	'Normal data again'

	; iterated data #2
	dw	32 dup(1234h,16 dup(0ABCDh,09876h))

	db	'Normal data again #2'

	org	0

_hello:
	db	'Hello world'

	org	8000h

_hello2:
	db	'And here'

	PROG ends

	end _start

