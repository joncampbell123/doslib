title TestOE
.8086
.model tiny
jumps
.code
org 100h
Start:
	;Set mode
	mov ax, 13h
	int 10h
	;Graphic controller - enable Odd/Even addressing
	mov dx, 3CEh
	mov al, 06h
	out dx, al
	inc dx
	in al, dx
	or al, 02h
	out dx, al
	;Sequencer: disable Chain4, enable Odd/Even	
	mov dx, 3C4h
	mov al, 04h
	out dx, al
	inc dx
	in al, dx
	and al, 0F3h
	out dx, al
	;Prepare to write map mask
	dec dx
	mov al, 02h
	out dx, al
	inc dx
	;Prepare memory
	mov ax, 0A000h
	mov es, ax
	cld
	;Write even planes, low page - blue and green
	mov al, 03h
	out dx, al
	mov cx, 16000
	xor di, di
a:
	mov ax, 0201h
	stosw
	add di, 2
	loop a
	;Write odd planes, low page - red and gray
	mov al, 0Ch
	out dx, al
	mov cx, 16000
	xor di, di
b:
	mov ax, 0704h
	stosw
	add di, 2
	loop b	
	;Pause, press any key to continue
	xor ax, ax
	int 16h
	;Return to text mode
	mov ax, 03h
	int 10h
	;Terminte
	ret
end Start
