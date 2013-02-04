bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

; beep at 440Hz
TONE		EQU	(1193180 / 440)
; for 1 second (well... as close to 1 second as we can!)
TICKS		EQU	18

; assume timer tick is at usual 18.2 ticks/sec

; program timer 2
		mov	dx,42h
		mov	al,0xB6
		out	43h,al
		mov	al,(TONE) & 0xFF
		out	dx,al
		mov	al,(TONE) >> 8
		out	dx,al
		inc	dl
		mov	al,0x24		; counter=0 most significant byte only interrupt on TC
		out	dx,al		; DX=43h

; load pc speaker control, save original gate value
		in	al,61h
		push	ax
		or	al,3
		out	61h,al

; wait each tick out, one complete timer cycle
		mov	cl,TICKS
		mov	dl,40h		; DX=40h
waitloop:	call	fetch
		jz	waitloop	; loop while hibyte is zero
waitloop2:	call	fetch
		jnz	waitloop2
		loop	waitloop

; restore timer
		mov	al,0x64		; counter=0 LSB-MSB interrupt on TC
		out	43h,al

; restore gate
		pop	ax
		out	61h,al

; exit folded into wait loop fetch
fetch:		in	al,dx
		or	al,al
		ret			; a way to return to DOS


