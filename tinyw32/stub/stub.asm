bits 16			; 16-bit real mode

segment code CLASS=CODE

..start:	mov	ax,4C00h
		int	21h

segment stack CLASS=STACK

