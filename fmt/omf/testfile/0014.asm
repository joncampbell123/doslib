
	PROG segment para public 'CODE'
	assume cs:PROG
	assume ds:PROG

	public	_start
	public	_hello
	public	_hello2
	public	_iter1
	public	_iter2
	public	_iter3

	org	400h

	dw	_iter2
	dw	_iter3

	nop
_start:	mov	ax,4C00h
	int	21h
	ret

	dw	32 dup(1234h)

	; iterated data
_iter1:
	dw	512 dup(_start)

	db	'Normal data'

	; iterated data #2
_iter2:
	dw	32 dup(_start,_hello)

	db	'Normal data again'

	dw	16 dup(0ABCDh,01234h)

	; iterated data #2
_iter3:
	dw	32 dup(_start,4 dup(_hello,_hello2))

	db	'Normal data again #2'

	org	200h

	dw	_iter1

_hello:
	db	'Hello world'

	org	8000h

	dw	_iter3

_hello2:
	db	'And here'

	PROG ends

	DATA segment para public 'DATA'
	assume cs:DATA
	assume ds:DATA

	org	200h

	db	90h
_dsym:	dw	1234h

	DATA ends

	DGROUP GROUP PROG, DATA

	end _start

