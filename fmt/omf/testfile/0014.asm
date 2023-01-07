
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

	nop
_start:	mov	ax,4C00h
	int	21h
	ret

	; iterated data
_iter1:
	dw	512 dup(_start)

	db	'Normal data'

	; iterated data #2
_iter2:
	dw	32 dup(_start,_hello)

	db	'Normal data again'

	; iterated data #2
_iter3:
	dw	32 dup(_start,4 dup(_hello,_hello2))

	db	'Normal data again #2'

	org	100h

_hello:
	db	'Hello world'

	org	8000h

_hello2:
	db	'And here'

	PROG ends

	end _start

