bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

segment .code

; ==== Reach into PSP segment, commad line, zero-chop at first space or newline
		mov	si,0x80
		mov	bl,[si]
		xor	bh,bh
		inc	si
		lea	bx,[si+bx]
		mov	byte [bx],bh	; BH=0, SI=0x81
		inc	si

; OK. Open file. Name is given now in DS:SI
		xor	cx,cx
		mov	dx,si		; INT 21h name DS:DX
		mov	ax,0x3D00	; Function 3Dh, mode=readonly
		int	21h
		jnc	file_ok

		mov	dx,file_err
		mov	ah,0x09
		int	21h

		mov	ax,0x4C01
		int	21h		; exit error

; file ok
file_ok:	mov	[file_handle],ax

; read loop
read_loop:	mov	ah,0x3F
		mov	bx,[file_handle]
		mov	cx,16
		mov	dx,temp16
		int	21h
		jc	end_of_file
		or	ax,ax
		jz	end_of_file
		; AX=bytes actually read
		mov	cx,ax
		mov	si,dx		; we trust DOS does not modify DX
		mov	word [temphx+2],0x2420
read_loop_bp:	lodsb
		call	hex_al
		mov	ah,0x09
		mov	dx,temphx
		int	21h

		loop	read_loop_bp

		mov	ah,0x09
		mov	dx,newline
		int	21h

		jmp	read_loop

; close file
end_of_file:	mov	bx,[file_handle]
		mov	ah,0x3E
		int	21h

		ret			; a way to return to DOS

hex_al:		xor	bh,bh

		mov	bl,al
		shr	bl,4		; <- OOPS! This won't run below 286 now :)
		mov	bl,[hexes+bx]
		mov	[temphx+0],bl

		mov	bl,al
		and	bl,0xF
		mov	bl,[hexes+bx]
		mov	[temphx+1],bl

		ret

newline		db	13,10,'$'
file_err	db	'Cannot open','$'
hexes		db	'0123456789ABCDEF'

segment .bss ; <--- VARIABLES BEYOND THIS POINT ARE NOT IN THE COM FILE IMAGE
; file handle:
file_handle:	resw	1
; at this location: temporary buffer 16 bytes
temp16:		resb	16
; str
temphx:		resb	4

