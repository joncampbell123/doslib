bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

segment .code

buffer_size	equ	16384
timer_clk	equ	1193180

; interesting facts:
; Most DOS systems leave the registers cleared to zero upon entry here.
; A Windows 98 bootdisk session with DEBUG.COM however shows that MS-DOS likes to leave CX = number of bytes it read from the COM file.
;
; SO: This code assumes that (Windows 98 style) AX=0 BX=0 CX=size DX=<nonzero unless of course run under DEBUG.COM> SI=0 DI=0
;
; covoxwav.com <WAV file>
;
; WARNING: Running this under DOSBox will give you the false impression the Disney/Covox is actually capable of running
;          up to 48KHz---it's NOT. It might be possible though if you run this on a Pentium and wire up your own LPT DAC,
;          depending on your motherboard chipset.
		mov	bl,0x80
		add	bl,[bx]
		cmp	bl,0x82			; if the command line was empty, then echo STDIN
		jae	short argv_open		; BL >= 0x82, open the file named
		ret				; else exit now

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

		; read the first 44 bytes, detect WAVE header, get sample rate
		mov	ah,0x3F			; read file
		mov	cx,44
		mov	dx,buffer
		int	21h			; BX=handle. INT 21h AH=0x3F does not modify BX, CX, DX
		jc	end_of_file
		or	ax,ax
		jz	end_of_file

		call	detect_wav_header
	
		mov	dx,timer_clk >> 16
		mov	ax,timer_clk & 0xFFFF
hackme_srate:	mov	cx,8000			; 8000Hz rate. In self-modifying code fashion, WAV reader will overwrite the 16-bit immediate
						; B9401F            mov cx,0x1f40
						;   ~~~~    <- overwrites that part
		div	cx
		mov	[ratclk],ax

read_loop:	mov	ah,0x3F			; read file
		mov	cx,buffer_size
		mov	dx,buffer
		int	21h			; BX=handle. INT 21h AH=0x3F does not modify BX, CX, DX
		jc	end_of_file
		or	ax,ax
		jz	end_of_file
		mov	cx,ax			; put return byte count into CX, for loop below
		mov	si,dx			; DOS doesn't modify DX on return

		cli

		mov	dx,[ratclk]		; set timer faster, to sample rate
		mov	di,dx			; we'll need DX/2 for below
		shr	di,1
		call	settimer

		clc
		push	bx
		mov	dx,0x378		; DX=covox/disney port, CX=bytes read, SI=buffer

.ploop:		lodsb				; load byte and write
		out	dx,al

.twait1:	call	readtimer		; wait for timer to tick down >= DX/2
		cmp	bx,di
		jae	.twait1

.twait2:	call	readtimer		; then wait for timer to tick the rest of the way down
		cmp	bx,di
		jl	.twait2

		loop	.ploop
		pop	bx

		mov	dx,0			; restore 18.2 timer tick
		call	settimer

		sti

		mov	ah,1			; check keyboard
		int	16h
		jz	short read_loop
		cmp	al,27
		jnz	short read_loop

end_of_file:	mov	ah,0x3E			; close file
		int	21h
		ret				; exit

; DX=tick rate. Does not modify BX/CX
settimer:	mov	al,0x34			; timer=0 read/write=LSB+MSB rate gen
		out	43h,al
		mov	al,dl
		out	40h,al
		mov	al,dh
		out	40h,al
		ret

; returns BX = timer counter
readtimer:	mov	al,0x04			; timer=0 read/write=LSB+MSB rate gen
		out	43h,al
		in	al,40h
		mov	bl,al
		in	al,40h
		mov	bh,al
		inc	bx			; some impl. might return 0xFFFF if at TC
		ret

; detect WAV header, read sample rate.
; 44 bytes of "buffer" contain what might be a header
detect_wav_header:
; look for "WAVEfmt" at buffer+8
		mov	si,buffer
		cmp	word [si+8],0x4157	; 'WA'
		jnz	.exit
		cmp	word [si+10],0x4556	; 'VE'
		jnz	.exit
		cmp	word [si+12],0x6D66	; 'fm'
		jnz	.exit
		cmp	word [si+14],0x2074	; 't '
		jnz	.exit

; buffer+16 DWORD = fmt chunk size
; buffer+20 WORD = compression code
; buffer+22 WORD = number of channels
; buffer+24 DWORD = sample rate
		mov	ax,word [si+24]		; read lower 16 bits of sample rate
		mov	[hackme_srate+1],ax	; and overwrite the immediate part of the "MOV CX,8000" opcode
						; Mmmmmm.... self-modifying code

.exit		ret

segment .bss
buffer		resb	buffer_size
ratclk		resw	1

