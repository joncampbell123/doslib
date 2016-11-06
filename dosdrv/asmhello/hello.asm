; MS-DOS device driver "hello"
; .COM type
bits 16			; 16-bit real mode
org 0			; MS-DOS .SYS style

; =================== HEADER ====================
dw	0xFFFF,0xFFFF	; next device ptr
dw	0xA800		; bit 15: 1=character device
			; bit 14: 0=does not support IOCTL control strings
			; bit 13: 0=does not support output until busy
			; bit 11: 1=understands open/close
			; bit  6: 0=does not understand 3.2 functions
			; bit 3:0: 0=not NUL, STDIN, STDOUT, CLOCK, etc.
dw	drv_strategy	; offset of strategy routine
dw	drv_interrupt	; offset of interrupt routine
db	'HELLO   '	; device name
db	0

; var storage
req_pkt:dd 0

; =================== Strategy routine ==========
drv_strategy:
	mov		[cs:req_pkt+0],bx		; BYTE ES:BX+0x00 = length of request header
	mov		[cs:req_pkt+2],es		; BYTE ES:BX+0x01 = unit number
	retf						; BYTE ES:BX+0x02 = command code
							; WORD ES:BX+0x03 = driver return status word
							;      ES:BX+0x05 = reserved??

; =================== Interrupt routine =========
drv_interrupt:
	push		ax
	push		bx
	push		si
	push		ds
	push		es
	les		bx,[cs:req_pkt]			; ES:BX MS-DOS request
	mov		al,[es:bx+2]			; so, what request?

	cmp		al,17				; request #16 or less
	jae		.out_of_range		

	xor		ah,ah
	shl		ax,1
	mov		si,ax				; SI = AL * 2
	call		word [cs:func_table+si]
.return:
	pop		es
	pop		ds
	pop		si
	pop		bx
	pop		ax
	retf

.out_of_range:
	mov		word [es:bx+3],0x810C		; general error/done
	jmp short	.return

; ===================== func table ================
func_table:	dw	func_init			; 0x00 INIT
		dw	ignore				; 0x01 MEDIA CHECK
		dw	bad_request			; 0x02 BUILD BIOS BPB
		dw	bad_request			; 0x03 I/O read
		dw	read				; 0x04 READ
		dw	nd_read				; 0x05 NON-DESTRUCTIVE READ
		dw	input_status			; 0x06 INPUT STATUS
		dw	ignore				; 0x07 FLUSH INPUT BUFFERS
		dw	write				; 0x08 WRITE
		dw	write				; 0x09 WRITE WITH VERIFY
		dw	output_status			; 0x0A OUTPUT STATUS
		dw	ignore				; 0x0B FLUSH OUTPUT BUFFERS
		dw	bad_request			; 0x0C I/O write
		dw	open				; 0x0D open
		dw	close				; 0x0E close
		dw	bad_request			; 0x0F REMOVABLE MEDIA CALL
		dw	bad_request			; 0x10 OUTPUT UNTIL BUSY

func_init:						; 0x00 INIT
							;  BYTE ES:BX+0x0D = number of units initialized
							; DWORD ES:BX+0x0E = return break address (last address of driver)
							; DWORD ES:BX+0x12 = pointer to the character on the CONFIG.SYS line following DEVICE=
							;                    block devices return FAR pointer to BIOS param block here
							;  BYTE ES:BX+0x16 = drive number (A=0, B=1) for first unit of block driver (MS-DOS 3+)
		mov		word [es:bx+3],0x0100	; status: no error, done
		mov		word [es:bx+14],end_resident	; change "last address in driver"
		mov		word [es:bx+16],cs	; NTS: we MUST change this, else Windows 95 chokes
		ret

ignore:							; 0x01 MEDIA CHECK
							; 0x07 FLUSH INPUT BUFFERS
							; 0x0B FLUSH OUTPUT BUFFERS
		mov		word [es:bx+3],0x0100
		ret

open:
close:
		mov		word [cs:data_fp],data
							; 0x0D open
							; 0x0E close
		mov		word [es:bx+3],0x0100
		ret

bad_request:						; 0x02 BUILD BIOS BPB
							; 0x03 I/O read
							; 0x0C I/O write
							; 0x0F REMOVABLE MEDIA CALL
							; 0x10 OUTPUT UNTIL BUSY
		mov		word [es:bx+3],0x8103
		ret

read:							; 0x04 READ
							;  BYTE ES:BX+0x0D = media descriptor byte
							; DWORD ES:BX+0x0E = far pointer to buffer to read to
							;  WORD ES:BX+0x12 = count of bytes to read, on return, count of bytes read
							;  WORD ES:BX+0x14 = starting block number (block devices)
							; DWORD ES:BX+0x16 = far pointer to volume label if error 15 reported
		mov		word [es:bx+3],0x0100
		push		cx
		push		di
		push		bp
		xor		bp,bp			; BP = return byte counter
		lds		di,[es:bx+0x0E]		; DS:DI = caller's buffer
		mov		cx,[es:bx+0x12]		; CX = count of bytes to read
.rloop:		jcxz		.end
		mov		si,word [cs:data_fp]
		mov		al,[cs:si]
		mov		byte [di],al
		inc		di			; DS:DI++
		dec		cx			; count--
		inc		bp			; BP++
		inc		word [cs:data_fp]
		cmp		word [cs:data_fp],data_eof
		jnz		.rloop
		mov		word [cs:data_fp],data
		jmp		.rloop
.end:		mov		[es:bx+0x12],bp
		pop		bp
		pop		di
		pop		cx
		ret

nd_read:						; 0x05 NON-DESTRUCTIVE READ
							;  BYTE ES:BX+0x0D = return char
		mov		si,word [cs:data_fp]
		mov		al,[cs:si]
		mov		byte [es:bx+0x0D],al
		mov		word [es:bx+3],0x0100
		ret

write:							; 0x08 WRITE
							; 0x09 WRITE WITH VERIFY
		mov		word [es:bx+3],0x8105
		ret

input_status:						; 0x06 INPUT STATUS
		mov		word [es:bx+3],0x8105
		ret

output_status:						; 0x0A OUTPUT STATUS
		mov		word [es:bx+3],0x8105
		ret

data_fp:	dw		data
data:		db		"Hello world! First MS-DOS device driver!",13,10
data_eof:	equ		$-$$

end_resident:	equ		$-$$

