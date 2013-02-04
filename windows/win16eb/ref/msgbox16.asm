
	bits	16
	org	0

	; hwnd
	mov	ax,0x1234
	push	ax
	; LPCSTR far ptr
	mov	ax,0x1234	; offset
	push	ax
	mov	ax,0x5678	; seg
	push	ax
	; LPCSTR far ptr
	mov	ax,0x1234	; offset
	push	ax
	mov	ax,0x5678	; seg
	push	ax
	; fuStyle WORD
	mov	ax,0x1234
	push	ax
	; call MessageBox
	call	0x1234:0x5678

	retf

