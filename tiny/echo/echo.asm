bits 16			; 16-bit real mode
org 0x100		; MS-DOS .COM style

segment .code

; interesting facts:
; Most DOS systems leave the registers cleared to zero upon entry here.
; A Windows 98 bootdisk session with DEBUG.COM however shows that MS-DOS likes to leave CX = number of bytes it read from the COM file.
;
; SO: This code assumes that (Windows 98 style) AX=0 BX=0 CX=size DX=<nonzero unless of course run under DEBUG.COM> SI=0 DI=0
;
; Known bugs: If the command line is completely empty the '$' terminator code will completely miss the string, and we'll pring junk.
;             The Command interpreter will by default want to use it's own internal "echo" command. Make sure you run THIS version
;             by typing .\ECHO <whatever>
;
; COM file size = 16 bytes
		mov	bl,0x80
		add	bl,[bx]
		mov	byte [bx+1],'$'
		mov	ah,9
		mov	dx,0x82
		int	21h
		ret

