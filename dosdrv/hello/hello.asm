; MS-DOS device driver "hello"
; .COM type
bits 16			; 16-bit real mode
org 0			; MS-DOS .SYS style

; =================== HEADER ====================
dw	0xFFFF,0xFFFF	; next device ptr
dw	0xA000		; bit 15: 1=character device
			; bit 14: 0=does not support IOCTL control strings
			; bit 13: 0=does not support output until busy
			; bit 11: 0=does not understand open/close
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
	mov		[cs:req_pkt+0],bx
	mov		[cs:req_pkt+2],es
	retf

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
		dw	ignore				; 0x02 BUILD BIOS BPB
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
		dw	ignore				; 0x0D open
		dw	ignore				; 0x0E close
		dw	bad_request			; 0x0F REMOVABLE MEDIA CALL
		dw	bad_request			; 0x10 OUTPUT UNTIL BUSY

func_init:						; 0x00 INIT
		mov		word [es:bx+3],0x0100
		mov		word [es:bx+14],end_resident
		mov		word [es:bx+16],cs
		ret

ignore:							; 0x01 MEDIA CHECK
							; 0x02 BUILD BIOS BPB
							; 0x07 FLUSH INPUT BUFFERS
							; 0x0B FLUSH OUTPUT BUFFERS
							; 0x0D open
							; 0x0E close
		mov		word [es:bx+3],0x0100
		ret

bad_request:						; 0x03 I/O read
							; 0x0C I/O write
							; 0x0F REMOVABLE MEDIA CALL
							; 0x10 OUTPUT UNTIL BUSY
		mov		word [es:bx+3],0x8103
		ret

read:							; 0x04 READ
		mov		word [es:bx+3],0x8105
		ret

nd_read:						; 0x05 NON-DESTRUCTIVE READ
		mov		word [es:bx+3],0x8105
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

end_resident:	equ		$-$$

