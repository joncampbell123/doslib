bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

		mov	ah,2
		mov	dl,7
		int	21h
		ret			; a way to return to DOS

