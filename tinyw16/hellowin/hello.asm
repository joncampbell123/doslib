bits 16			; 16-bit protected mode

section text class=CODE

extern POSTQUITMESSAGE
extern INITTASK
extern INITAPP
extern WAITEVENT
extern REGISTERCLASS
extern DEFWINDOWPROC
extern CREATEWINDOW
extern SHOWWINDOW
extern UPDATEWINDOW
extern GETMESSAGE
extern TRANSLATEMESSAGE
extern DISPATCHMESSAGE
extern MAKEPROCINSTANCE
extern GETSTOCKOBJECT
extern LOADCURSOR
extern LOADICON

..start:
		call far INITTASK
		; ^ input: AX=0 (or something else in Windows 2.x???)
		;          BX=stack size
		;          CX=heap size
		;          DX=unused
		;          SI=previous instance handle
		;          DI=instance handle
		;          BP=0
		;          DS=app data segment
		;          ES=selector of program segment prefix
		;          SS=app data segment
		;          SP=top of stack
		;
		;   output: AX=selector of program segment prefix or 0 on error
		;           BX=offset of command line
		;           CX=stack limit
		;           DX=nCmdShow (to be passed to WinMain)
		;           SI=previous instance handle
		;           DI=instance handle
		;           BP=top of stack
		;           DS=app data segment
		;           ES=selector of command line
		;           SS=app data segment (SS==DS)
		;           SP=edited top of stack
		or	ax,ax
		jz	_exit	; if AX==0
		
		mov	[myInstance],di
		mov	[myWNDCLASS_hInstance],di

		push	word 0
		call far WAITEVENT	; FIXME: Why does Watcom CRT startup do this?

		push	di		; InitApp(hInstance)
		call far INITAPP	; NTS: Microsoft Windows 1.04 will crash to DOS *HERE* if Watcom sets the stack size below 512 bytes
		or	ax,ax
		jz	_exit

		; FARPROC MakeProcInstance(FARPROC,HINSTANCE)
		push	cs			; FARPROC
		push	myWndProc

		push	word [myInstance]

		call far MAKEPROCINSTANCE	; returns FARPROC in dx:ax

		mov	[myWNDCLASS_wndProc],ax
		mov	[myWNDCLASS_wndProc+2],dx

		; Real-mode windows: update data segment pointer
		mov	word [myWNDCLASS_classname],myWNDCLASSName
		mov	[myWNDCLASS_classname+2],ds

		; We need IDI_APPLICATION and IDC_ARROW or Windows 1.0 will not let us create the window class
		; LoadIcon(HINSTANCE hInstance,LPCSTR lpIconName)
		push	word 0	        ; hInstance    = NULL
		push	word 0			; lpIconName   \-----   MAKEINTRESOURCE(IDI_APPICON)
		push	word 32512		; IDI_APPICON  /-----
		call far LOADICON
		mov	word [myWNDCLASS_hIcon],ax
		or	ax,ax
		jz	_exit

		; LoadCursor(HINSTANCE hInstance,LPCSTR lpCursorName)
		push	word 0	        ; hInstance    = NULL
		push	word 0			; lpIconName   \------  MAKEINTRESOURCE(IDC_ARROW) 
		push	word 32512		; IDC_ARROW    /------
		call far LOADCURSOR
		mov	word [myWNDCLASS_hCursor],ax
		or	ax,ax
		jz	_exit

		; GetStockObject(WORD obj)
		push	word 0					; WHITE_BRUSH
		call far GETSTOCKOBJECT
		mov	word [myWNDCLASS_hbrBackground],ax
		or	ax,ax
		jz	_exit

		; RegisterClass(myWNDCLASS)
		push	ds
		push	word myWNDCLASS
		call far REGISTERCLASS				; <-- FIXME: Windows 1.04 fails this call. Why?
        int3
		or	ax,ax
		jz	_exit
		mov	[myWNDCLASSAtom],ax

		; CreateWindow(...)
		push	ds			; LPCSTR lpszClassName
		push	word myWNDCLASSName

		push	ds			; LPCSTR lpszWindowName
		push	word HelloTitle

		; WS_OVERLAPPED =	0x00000000 (ALSO WS_TILEDWINDOW under Windows 1.0)
		; WS_CAPTION =		0x00C00000
		; WS_SYSMENU =		0x00080000
		; WS_THICKFRAME =	0x00040000
		; WS_MINIMIZEBOX =	0x00020000
		; WS_MAXIMIZEBOX =	0x00010000
		push	word 0x00CF		; DWORD dwStyle (HI)
		push	word 0x0000		; DWORD dwStyle (LO)

		push	word 0			; x

		push	word 0			; y

		push	word 480		; nWidth

		push	word 100		; nHeight

		push	word 0			; hwndParent

		push	word 0			; hmenu

		push	word [myInstance]	; hInstance

		push	word 0			; lpvParam
		push	word 0

		call far CREATEWINDOW
		or	ax,ax
		jz	_exit
		mov	[myHwnd],ax		; return value = window handle

		; show the window
		push	word [myHwnd]		; our window handle
%if TARGET_WINDOWS == 10
		push	word 1			; SHOW_OPENWINDOW (Windows 1.x)
%else
		push	word 5			; SW_SHOW (Windows 2.x and higher)
%endif
		call far SHOWWINDOW

		; update the window
		push	word [myHwnd]
		call far UPDATEWINDOW

; --------------------------MESSAGE PUMP-----------------------------------
pumploop:
;     WORD _pascal GetMessage(MSG FAR *msg,HWND hwnd,UINT uMsgFilterMin,UINT uMsgFilterMax);
		push	ds
		push	word myMSG
		push	word 0			; any message belonging to this app
		push	word 0x0000
		push	word 0xFFFF
		call far GETMESSAGE
		or	ax,ax
		jz	_exit

		push	ds
		push	word myMSG
		call far TRANSLATEMESSAGE

		push	ds
		push	word myMSG
		call far DISPATCHMESSAGE

		jmp	pumploop

_exit:		mov	ax,4C00h
		int	21h

;---------------------------------------------------------------------------------------------
; window procedure
; LRESULT _pascal WindowProc(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam);
;  in: [bp+2+0] = DWORD FAR RETURN
;      [bp+2+4] = DWORD lparam
;      [bp+2+8] = WORD wparam
;      [bp+2+10] = WORD msg
;      [bp+2+12] = WORD hwnd
;       = bp+2+14 max
;       = 10 bytes of params
;  out: AX = return value
global myWndProc
myWndProc:	push	ds
		pop	ax
		nop
		inc	bp			; we are FAR proc (apparently that's what this means?!?!)
		push	bp
		mov	bp,sp
		push	ds
		mov	ds,ax

		mov	ax,[bp+2+10]
		cmp	ax,0x0001		; WM_CREATE?
		jz	.wm_create
		cmp	ax,0x0002		; WM_DESTROY
		jz	.wm_destroy

; call DefWindowProc(HWND hwnd,MSG msg,WPARAM wparam,LPARAM lparam)
		push	word [bp+2+12]		; hwnd
		push	word [bp+2+10]		; msg
		push	word [bp+2+8]		; wparam
		push	word [bp+2+6]		; lparam[1]
		push	word [bp+2+4]		; lparam[2]
		call far DEFWINDOWPROC
		; let AX return value alone, to return to caller

.exit:		pop	ds
		pop	bp
		dec	bp
		retf	10			; _pascal return for 10 bytes of params on stack

.wm_create:	xor	ax,ax			; return zero
		jmp	.exit
.wm_destroy:	push	word 0			; exit code
		call far POSTQUITMESSAGE
		xor	ax,ax
		jmp	.exit
;-------------------------------------------------------------------------------------------------

segment data class=DATA

inittask_dgroup:	; DGROUP first 16 bytes used by Windows
		dw	0,0,5,0, 0,0,0,0
		; ^ According to WINE sources this is initialized and used during execution:
		;    WORD	0
		;    DWORD      Old SS:SP
		;    WORD       pointer to local heap info if any
		;    WORD       pointer to local atom table if any
		;    WORD       top of stack
		;    WORD       lowest stack addr so far
		;    WORD       bottom of the stsack
		;    = 8 WORDs
		;
		;   Now someone explain why it is initialized in the EXE image to "Old SS:SP" = 0005:0000....

myWNDCLASS:
	; WNDCLASS:
	;    UINT	style				[WORD]
		dw	0x0002|0x0001			; CS_HREDRAW|CS_VREDRAW
	;    WNDPROC	lpfnWndProc			[DWORD = FAR POINTER]
myWNDCLASS_wndProc:
		dw	0
		dw	0
	;    int	cbClsExtra			[WORD]
		dw	0
	;    int	cbWndExtra			[WORD]
		dw	0
	;    HINSTANCE	hInstance			[WORD]
myWNDCLASS_hInstance:
		dw	0
	;    HICON	hIcon				[WORD]
myWNDCLASS_hIcon:
		dw	0
	;    HCURSOR	hCursor				[WORD]
myWNDCLASS_hCursor:
		dw	0
	;    HBRUSH	hbrBackground			[WORD]
myWNDCLASS_hbrBackground:
		dw	0				; (HBRUSH)(COLOR_WINDOW+1) = (5+1)
	;    LPCSTR	lpszMenuName			[DWORD = FAR POINTER]
		dw	0
		dw	0
	;    LPCSTR	lpszClassName			[DWORD = FAR POINTER]
myWNDCLASS_classname:
		dw	0
		dw	0
	;      = 13 WORDs

myWNDCLASSName:	db	"HELLOWORLDASM",0
HelloText:	db	"Hello World!",0
HelloTitle:	db	"Title",0

segment bss

myInstance:	resw	1
myWNDCLASSAtom:	resw	1
myHwnd:		resw	1
myMSG: ; MSG struct
		resw	1				; HWND
		resw	1				; message
		resw	1				; wparam
		resd	1				; lparam
		resd	1				; time
		resd	1				; POINT pt {int x; int y}

; for whatever reason, this is MANDATORY, or else the NE header will cause
; windows to call our code with a data segment consisting of the first bytes
; of this EXE! and INITTASK won't work. there MUST be a DGROUP! Probably
; a strange Watcom Linker bug too, who knows?
group dgroup data bss

