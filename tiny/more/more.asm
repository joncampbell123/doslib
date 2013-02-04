bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

segment .code

buffer_size	equ	4096

; interesting facts:
; Most DOS systems leave the registers cleared to zero upon entry here.
; A Windows 98 bootdisk session with DEBUG.COM however shows that MS-DOS likes to leave CX = number of bytes it read from the COM file.
;
; SO: This code assumes that (Windows 98 style) AX=0 BX=0 CX=size DX=<nonzero unless of course run under DEBUG.COM> SI=0 DI=0
;     and CS == DS == ES
;
; How to use:
;
; <command> | more
;
; or
;
; more <file.txt
reentry:	mov	byte [linecount],24

readloop:	mov	ah,3Fh			; READ FILE OR DEVICE
		xor	bx,bx			; STDIN
		mov	cx,1			; 1 byte
		mov	dx,charc		; char * = &charc
		int	21h
		jc	short end_of_file	; CF=1 error
		or	ax,ax
		jz	short end_of_file	; AX=0 EOF

		mov	ah,40h			; WRITE FILE OR DEVICE (preparation!)
		inc	bx			; BX=1 (BX remained 0 through INT 21h)

		mov	al,byte [charc]
		cmp	al,10			; byte was LF?
		jz	short linefeed		; go to linefeed logic, counting lines and waiting for enter
		cmp	al,13			; byte was CR?
		jz	short readloop		; don't print it

		int	21h			; prior INT 21h kept BX, CX, DX

		jmp	short readloop

; char was linefeed, count line and emit CRLF
linefeed:	mov	dx,crlf
		mov	cl,2
		int	21h

		dec	byte [linecount]
		jnz	short readloop

		; pause, wait for keypress
.waitforenter:	xor	ah,ah
		int	16h
		cmp	al,27			; ESC
		jz	end_of_file
		cmp	al,13			; anything other than ENTER does nothing
		jnz	.waitforenter
		jmp	short reentry

end_of_file:	ret				; exit

crlf		db	13,10

segment .bss
linecount	resb	1
charc		resb	1

