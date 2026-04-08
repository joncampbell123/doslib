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
	;Graphic controller - enable Odd/Even addressing, set 128Kb mode
	mov dx, 3CEh
	mov al, 06h
	out dx, al
	inc dx
	in al, dx
	and al, 0F3h
	or al, 02h
	out dx, al
	;CRTC - disable Chain4 and Odd/Even
	mov dx, 3D4h
	mov al, 14h
	out dx, al
	inc dx
	in al, dx
	and al, 0BFh
	out dx, al
	dec dx
	mov al, 17h
	out dx, al
	inc dx
	in al, dx
	or al, 40h
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
	mov cx, 8000
	xor di, di
	mov ax, 0201h
	rep stosw
	;Write odd planes, low page - red and gray
	mov al, 0Ch
	out dx, al
	mov cx, 8000
	xor di, di
	mov ax, 0704h
	rep stosw
	;Prepare to write to high page
	mov ax, 0B000h
	mov es, ax
	;Write even planes, high page - light blue and light green
	mov al, 03h
	out dx, al
	mov cx, 8000
	xor di, di
	mov ax, 0A09h
	rep stosw
	;Write odd planes, high page - ligh red and white
	mov al, 0Ch
	out dx, al
	mov cx, 8000
	xor di, di
	mov ax, 0F0Ch
	rep stosw	
	;Pause, press any key to continue
	xor ax, ax
	int 16h
	;Return to text mode
	mov ax, 03h
	int 10h
	;Terminte
	ret
end Start
