; 1bpp Chain4 mode test. Both line patterns should be the same. Doesn't work in QEMU, but all of a sudden works in 86Box.
title TestS4
.8086
.model tiny
jumps
.code
org 100h
Start:
	;640x200x4
	mov ax, 000Eh
	int 10h
	;Render
	call Render
	;Pause, press any key to continue
	xor ax, ax
	int 16h
	;320x200x8
	mov ax, 0013h
	int 10h
	;Enable SC Shift4
	mov dx, 3C4h
	mov al, 01h
	out dx, al
	inc dx
	in al, dx
	or al, 10h
	out dx, al
	;Disable GC Shift4
	mov dx, 3CEh
	mov al, 05h
	out dx, al
	inc dx
	in al, dx
	and al, 0BFh
	out dx, al
	;Enable CRTC Shift4
	mov dx, 3D4h
	mov al, 14h
	out dx, al
	inc dx
	in al, dx
	or al, 20h
	out dx, al
	;Offset = 640 / (8 * 4 * 2) = 10
	dec dx
	mov ax, 0A13h
	out dx, ax
	;Render
	call Render
	;Pause, press any key to continue
	xor ax, ax
	int 16h
	;Return to text mode
	mov ax, 03h
	int 10h
	;Terminte
	ret
Render proc
	;Disable planes 1-3
	mov dx, 3DAh
	in al, dx
	mov dx, 3C0h
	mov ax, 0112h
	out dx, al
	mov al, ah
	out dx, al
	mov al, 20h
	out dx, al
	mov dx, 3DAh
	in al, dx
	;Draw vertial line pattern
	mov ax, 0A000h
	mov es, ax
	xor di, di
	cld
	mov cx, 10 * 200
@@a:
	mov ax, 0201h
	stosw
	mov ax, 0804h
	stosw
	mov ax, 2010h
	stosw
	mov ax, 8040h
	stosw
	loop @@a
	ret
endp
end Start
