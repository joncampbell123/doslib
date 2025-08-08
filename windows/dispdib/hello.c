#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

/* FIXME: Windows 3.1, 16-bit:
       This code manages to do a fullscreen display, but then for whatever reason, all keyboard
       input is disabled. Why? */

/* Windows programming notes:
 *
 *   - If you're writing your software to work only on Windows 3.1 or later, you can omit the
 *     use of MakeProcInstance(). Windows 3.0 however require your code to use it.
 *
 *   - The Window procedure must be exported at link time. Windows 3.0 demands it.
 *
 *   - The Window procedure must be PASCAL FAR __loadds if you want this code to work properly
 *     under Windows 3.0. If you only support Windows 3.1 and later, you can remove __loadds,
 *     though it's probably wiser to keep it.
 *
 *   - Testing so far shows that this program for whatever reason, either fails to load it's
 *     resources or simply hangs when Windows 3.0 is started in "real mode".
 */

#include <windows.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <time.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <hw/dos/dos.h>
#include <windows/apihelp.h>
#include <windows/dispdib/dispdib.h>
#include <windows/win16eb/win16eb.h>

HWND		hwndMain = NULL;
const char*	WndProcClass = "HELLOWINDOWS";
const char*	HelloWorldText = "If you can see this, DISPDIB.DLL is not active.\n\nAt any time, hit ENTER to activate fullscreen, ESC to leave.\nUse numeric keys at the top for pointer draw tests";
HINSTANCE	myInstance;
HICON		AppIcon;

/* If nonzero, specifies the clock tick at which this code should deactivate DISPDIB.DLL */
time_t		force_back_at = 0;

void DoDirectPtrDrawVGAText() {
	BYTE FAR *base;
	const char *message = "If you can see this, INT 10h modesetting works";
	unsigned int i;

	base = DisplayDibGetScreenPointerB800();
	if (base == NULL) {
		MessageBeep(-1);
		return;
	}

	for (i=0;i < (80*25);i++) {
		base[i*2    ] = i;
		base[i*2 + 1] = (((i/80)%15)+1)+((i/15/80)<<4);
	}

	for (i=0;message[i] != 0;i++)
		base[i*2] = message[i];
}

void DoDirectPtrDraw1() {
	unsigned int x,y,p,stride;
	BYTE FAR *base;

	base = DisplayDibGetScreenPointerA000();
	if (base == NULL) {
		MessageBeep(-1);
		return;
	}

	stride = DisplayDibScreenStride();
	if (DisplayDibScreenPointerIsModeX()) {
		for (p=0;p < 4;p++) { /* VGA "plane" to write to */
			outp(0x3C4,0x02); /* map mask register */
			outp(0x3C5,1 << p);
			for (y=0;y < dispDibFormat->biHeight;y++) {
				for (x=p;x < dispDibFormat->biWidth;x += 4) {
					base[(y*stride)+(x>>2)] = ((x+y)>>1) & (((x^y)&1)?0xFF:0x00);
				}
			}
		}
	}
	else {
		for (y=0;y < 200 && y < dispDibFormat->biHeight;y++) {
			for (x=0;x < dispDibFormat->biWidth;x++) {
				base[(y*stride)+x] = ((x+y)>>1) & (((x^y)&1)?0xFF:0x00);
			}
		}
	}
}

void DoDirectPtrDraw2() {
	unsigned int x,y,p,stride;
	BYTE FAR *base;

	base = DisplayDibGetScreenPointerA000();
	if (base == NULL) {
		MessageBeep(-1);
		return;
	}

	stride = DisplayDibScreenStride();
	if (DisplayDibScreenPointerIsModeX()) {
		for (p=0;p < 4;p++) { /* VGA "plane" to write to */
			outp(0x3C4,0x02); /* map mask register */
			outp(0x3C5,1 << p);
			for (y=0;y < dispDibFormat->biHeight;y++) {
				for (x=p;x < dispDibFormat->biWidth;x += 4) {
					base[(y*stride)+(x>>2)] = x^y;
				}
			}
		}
	}
	else {
		for (y=0;y < 200 && y < dispDibFormat->biHeight;y++) {
			for (x=0;x < dispDibFormat->biWidth;x++) {
				base[(y*stride)+x] = x^y;
			}
		}
	}
}

WindowProcType WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		SetTimer(hwnd,1,1000,NULL);
		return 0; /* Success */
	}
	else if (message == WM_TIMER) {
		time_t now = time(NULL);

		if (force_back_at != 0 && now >= force_back_at) {
			force_back_at = 0;
			dispDibUserActive = 0;
			DisplayDibDoEnd();
		}
	}
	else if (message == WM_DESTROY) {
		KillTimer(hwnd,1);
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_CHAR) {
		if (wparam == 27) {
			force_back_at = 0;
			if (DisplayDibDoEnd() == 0)
				dispDibUserActive = 0;
		}
		else if (wparam == 13) {
			if (DisplayDibDoBegin() == 0)
				dispDibUserActive = 1;
		}
		else if (wparam == '1') {
			if (dispDibFullscreen)
				DoDirectPtrDraw1();
		}
		else if (wparam == '2') {
			if (dispDibFullscreen)
				DoDirectPtrDraw2();
		}
		else if (wparam == '!') {
			DisplayDibDoEnd();
			if (DisplayDibSetFormat(320,200) != 0) MessageBeep(-1);
			DisplayDibGenerateTestImage();
			if (dispDibUserActive) DisplayDibDoBegin();
		}
		else if (wparam == '@') {
			DisplayDibDoEnd();
			if (DisplayDibSetFormat(320,240) != 0) MessageBeep(-1);
			DisplayDibGenerateTestImage();
			if (dispDibUserActive) DisplayDibDoBegin();
		}
		/* NTS: Windows 9x/ME DISPDIB.DLL recognizes 320x400 and 320x480 modes */
		else if (wparam == '#') {
			DisplayDibDoEnd();
			if (DisplayDibSetFormat(320,400) != 0) MessageBeep(-1);
			DisplayDibGenerateTestImage();
			if (dispDibUserActive) DisplayDibDoBegin();
		}
		else if (wparam == '$') {
			DisplayDibDoEnd();
			if (DisplayDibSetFormat(320,480) != 0) MessageBeep(-1);
			DisplayDibGenerateTestImage();
			if (dispDibUserActive) DisplayDibDoBegin();
		}
		else if (wparam == 'B') {
			DisplayDibDoEnd();
			if (DisplayDibSetFormat(320,200) != 0) MessageBeep(-1);
			DisplayDibDoBegin();
			if (dispDibFullscreen) {
				DisplayDibSetBIOSVideoMode(3);
				DoDirectPtrDrawVGAText();
				dispDibUserActive=1;
			}
		}
	}
	else if (message == WM_ACTIVATEAPP) {
		if (dispDibUserActive) {
			force_back_at = 0;
			if (wparam) DisplayDibDoBegin();
			else DisplayDibDoEnd();
		}
	}
	else if (message == WM_ERASEBKGND) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,FALSE)) {
			HBRUSH oldBrush,newBrush;
			HPEN oldPen,newPen;

			newPen = (HPEN)GetStockObject(NULL_PEN);
			newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

			oldPen = SelectObject((HDC)wparam,newPen);
			oldBrush = SelectObject((HDC)wparam,newBrush);

			Rectangle((HDC)wparam,um.left,um.top,um.right+1,um.bottom+1);

			SelectObject((HDC)wparam,oldBrush);
			SelectObject((HDC)wparam,oldPen);
		}

		return 1; /* Important: Returning 1 signals to Windows that we processed the message. Windows 3.0 gets really screwed up if we don't! */
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			PAINTSTRUCT ps;
			RECT rct;

			GetClientRect(hwnd,&rct);
			rct.top = 5+32+5+18;
			rct.left = 5;

			BeginPaint(hwnd,&ps);
			TextOut(ps.hdc,5,5+32+5,dispDibLoadMethod,strlen(dispDibLoadMethod));
			DrawText(ps.hdc,HelloWorldText,strlen(HelloWorldText),&rct,DT_LEFT|DT_TOP|DT_NOPREFIX|DT_NOCLIP|DT_WORDBREAK);
			DrawIcon(ps.hdc,5,5,AppIcon);
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	(void)lpCmdLine;

	myInstance = hInstance;

	/* FIXME: Windows 3.0 Real Mode: Why can't we load our icon? */
	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	if (!AppIcon) MessageBox(NULL,"Unable to load app icon","Oops!",MB_OK);

	/* NTS: In the Windows 3.1 environment all handles are global. Registering a class window twice won't work.
	 *      It's only under 95 and later (win32 environment) where Windows always sets hPrevInstance to 0
	 *      and window classes are per-application.
	 *
	 *      Windows 3.1 allows you to directly specify the FAR pointer. Windows 3.0 however demands you
	 *      MakeProcInstance it to create a 'thunk' so that Windows can call you (ick). */
	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = WndProc;
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = AppIcon;
		wnd.hCursor = NULL;
		wnd.hbrBackground = NULL;
		wnd.lpszMenuName = NULL;
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
	}

	hwndMain = CreateWindow(WndProcClass,"Hello!",
		WS_OVERLAPPED|WS_SYSMENU|WS_MINIMIZEBOX|WS_THICKFRAME,
		CW_USEDEFAULT,CW_USEDEFAULT,
		460,158,
		NULL,NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	if (DisplayDibLoadDLL())
		MessageBox(NULL,dispDibLastError,"Failed to load DISPDIB.DLL",MB_OK);

	if (DisplayDibCreateWindow(hwndMain))
		MessageBox(NULL,dispDibLastError,"Failed to create DispDib window",MB_OK);

	if (DisplayDibGenerateTestImage())
		MessageBox(NULL,dispDibLastError,"Failed to generate test card",MB_OK);

	/* NTS: I added this code out of paranoia after discovering that Windows 3.1 DISBDIB.DLL has what I
		would call some nasty n00b programming traps: You'll get your fullscreen VGA graphics, but
		apparently at the cost of being able to receive mouse and keyboard events. For the longest
		time, this manifested itself as the program successfully going fullscreen, then the user
		being unable to back out only to get "beeped" at by the Windows keyboard driver because the
		keyboard event queue was full, basically forcing the user to push RESET.

		I've since figured out how to avoid that, but if problems like that happen again, we want
		the user to at least have the assurance that their desktop will return in 8 seconds to show
		we're still alive. */
	force_back_at = time(NULL) + 8;
	if (DisplayDibDoBegin() == 0)
		dispDibUserActive = 1;
	else
		MessageBox(NULL,dispDibLastError,"Failed to begin display",MB_OK);

	while (GetMessage(&msg,NULL,0,0)) {
		if (DisplayDibCheckMessageAndRedirect(&msg) >= 0) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	DisplayDibDoEnd();
	dispDibUserActive = 0;
	if (DisplayDibUnloadDLL())
		MessageBox(NULL,dispDibLastError,"Failed to unload DISPDIB.DLL",MB_OK);

	return msg.wParam;
}

