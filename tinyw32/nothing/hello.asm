
; FIXME: Good lord! NASM + WLINK = 2048-byte PE image, mostly empty!

segment .code use32 flat align=16

global _WinMain
_WinMain:
; VOID WINAPI ExitProcess(UINT code)
		push	dword 0
		call	_ExitProcess@4

; entry
extern _ExitProcess@4

