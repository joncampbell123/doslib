
; FIXME: Good lord! NASM + WLINK = 2048-byte PE image, mostly empty!

segment .code use32 flat align=16

global _WinMain
_WinMain:

; int WINAPI MessageBoxA(HWND hwnd,LPCTSTR lpText,LPCTSTR lpCaption,UINT type)
        push    dword 0x00000000        ; MK_OK
        push    dword HelloCaption
        push    dword HelloText
        push    dword 0
        call    _MessageBoxA@16

; VOID WINAPI ExitProcess(UINT code)
        push    dword 0
        call    _ExitProcess@4

; strings
HelloCaption:   db  "Title",0
HelloText:      db  "Hello world!",0

; entry
extern _MessageBoxA@16
extern _ExitProcess@4

