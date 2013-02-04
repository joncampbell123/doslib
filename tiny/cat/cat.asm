bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

segment .code

buffer_size	equ	4096

; interesting facts:
; Most DOS systems leave the registers cleared to zero upon entry here.
; A Windows 98 bootdisk session with DEBUG.COM however shows that MS-DOS likes to leave CX = number of bytes it read from the COM file.
;
; SO: This code assumes that (Windows 98 style) AX=0 BX=0 CX=size DX=<nonzero unless of course run under DEBUG.COM> SI=0 DI=0
		mov	bl,0x80
		add	bl,[bx]
		cmp	bl,0x82			; if the command line was empty, then echo STDIN
		jae	short argv_open		; BL >= 0x82, open the file named
		xor	bl,bl			; else zero BX (STDIN handle) and jump into the loop
		jmp	short file_ok

argv_open:	mov	byte [bx+1],bh		; BH=0

		mov	ax,0x3D00		; function 3Dh read only
		mov	dx,0x82
		xor	cx,cx			; NTS experience says DOS enters here with CX = size of COM image, so we need to zero it
		int	21h
		jnc	file_ok

		mov	ax,0x4C01
		int	21h			; exit errcode=1

; file open worked, AX=handle
file_ok:	mov	bx,ax

read_loop:	push	bx
		mov	ah,0x3F			; read file
		mov	cx,buffer_size
		mov	dx,buffer
		int	21h
		pop	bx
		jc	end_of_file
		or	ax,ax
		jz	end_of_file

		push	bx
		mov	cx,ax			; CX=AX read count from above read call
		mov	ah,0x40			; write file
		mov	bx,1			; STDOUT
;		mov	dx,buffer		; our buffer
		int	21h
		pop	bx
		jmp	short read_loop

end_of_file:	mov	ah,0x3E			; close file
		int	21h
		ret				; exit

segment .bss
buffer		resb	buffer_size

