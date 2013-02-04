bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

		mov	ah,0x09
		mov	dx,ptr_string
		int	21h
		ret			; a way to return to DOS

ptr_string:	db	'Hello$'

