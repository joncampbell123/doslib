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

/* Explanation of this library:

   When compiled as Win16 code: This library provides a buffer that the calling program can write
   16-bit code to and execute.

   When compiled as Win32 code: This library provides a buffer that the calling program can write
   16-bit code to, and execute via Win32->Win16 thunking in Windows 9x/ME.

   WARNING: This library is intended for small one-off subroutines (such as a quick INT 10h call
   that Win32 cannot do directly). If your subroutine is complex it is recommended you write a
   Win16 DLL and use the Win9x thunk library code in hw/dos instead. */

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
#include <hw/dos/doswin.h>
#include <windows/apihelp.h>
#include <windows/win16eb/win16eb.h>

HWND		hwndMain = NULL;
const char*	WndProcClass = "HELLOWINDOWS";
const char*	HelloWorldText = "Use various keys to test Win16 executable code";
HINSTANCE	myInstance;
HICON		AppIcon;

int FillInitialNOPCode() {
	BYTE *buf;
	int i;

	buf = LockWin16EBbuffer();
	if (buf == NULL) return 0;

	/* 00000000  B83412            mov ax,0x1234 */
	buf[0] = 0xB8;
	buf[1] = 0x34;
	buf[2] = 0x12;
	/* NOPs */
	for (i=3;i < Win16EBbuffersize-1;i++) buf[i] = 0x90;
	/* RETF */
	buf[i] = 0xCB;

	UnlockWin16EBbuffer();
	return 1;
}

int TestExecuteInitialNOPCode() {
	BYTE FAR *buf;
	DWORD code,ret;
	char tmp[192];

	buf = LockWin16EBbuffer();
	if (buf == NULL) return 0;

	if (buf[3] != 0x90 || buf[Win16EBbuffersize-1] != 0xCB) {
		MessageBox(hwndMain,"Test NOP failure: Buffer got corrupted somehow, it doesn't contain the code I wrote","",MB_OK);
		UnlockWin16EBbuffer();
		return 0;
	}

	/* get code segment alias. note that the pointer is 16:16 far even in Win32 */
	code = GetCodeAliasWin16EBbuffer();
	if (code == 0) {
		MessageBox(hwndMain,"Test NOP failure: Unable to obtain code segment alias","",MB_OK);
		UnlockWin16EBbuffer();
		return 0;
	}

	/* OK, execute it */
	ret = ExecuteWin16EBbuffer(code);
	if (ret != 0x1234) {
		sprintf(tmp,"Test NOP failure: Code (probably) executed but return value is wrong (0x%08lX)",(unsigned long)ret);
		MessageBox(hwndMain,tmp,"",MB_OK);
	}

	UnlockWin16EBbuffer();
	return 1;
}

static void TextOutS(HDC hdc,int x,int y,const char *msg) {
	TextOut(hdc,x,y,msg,strlen(msg));
}

void INT10test() {
	HDC hdc;
	BYTE FAR *base,*p;
	DWORD code;

	hdc = GetDC(hwndMain);

	/* NTS: Don't worry, LockWin16EBbuffer() will not double-lock the buffer */
	TextOutS(hdc,0,0,"Locking buffer...");
	if ((base=p=LockWin16EBbuffer()) == NULL) {
		MessageBox(NULL,"Unable to lock buffer","",MB_OK);
		return;
	}

	/* force creation of the code alias */
	code = GetCodeAliasWin16EBbuffer();
	if (code == 0) {
		MessageBox(NULL,"Unable to get buffer code alias","",MB_OK);
		UnlockWin16EBbuffer();
		return;
	}

	/* MOV AX,3 */
	*p++ = 0xB8; *((WORD FAR*)p) = 0x0003; p+=2;
	/* INT 10h */
	*p++ = 0xCD; *p++ = 0x10;

	/* RETF */
	*p++ = 0xCB;

	/* run it! */
	ExecuteWin16EBbuffer(code);

	TextOutS(hdc,0,0,"NOP DONE                              ");
	ReleaseDC(hwndMain,hdc);
	UnlockWin16EBbuffer();

}

void NOPtest() {
	DWORD code;
	char tmp[128];

	HDC hdc = GetDC(hwndMain);

	/* NTS: Don't worry, LockWin16EBbuffer() will not double-lock the buffer */
	TextOutS(hdc,0,0,"Locking buffer...");
	if (LockWin16EBbuffer() == NULL) {
		MessageBox(NULL,"Unable to lock buffer","",MB_OK);
		return;
	}

	/* force creation of the code alias */
	code = GetCodeAliasWin16EBbuffer();
	if (code == 0) {
		MessageBox(NULL,"Unable to get buffer code alias","",MB_OK);
		UnlockWin16EBbuffer();
		return;
	}

	/* DEBUG: show the handles we obtained */
#if TARGET_MSDOS == 32
	sprintf(tmp,"size=%lu handle=%u %08X code=%04X",
		Win16EBbuffersize,
		Win16EBbufferhandle,
		Win16EBbufferdata16,
		Win16EBbuffercode16);
#else
	sprintf(tmp,"size=%lu handle=%u %04X:%04X %04X code=%04X",
		Win16EBbuffersize,
		Win16EBbufferhandle,
		FP_SEG(Win16EBbufferdata16),
		FP_OFF(Win16EBbufferdata16),
		Win16EBbufferdata16sel,
		Win16EBbuffercode16);
#endif
	TextOutS(hdc,0,20,tmp);

	TextOutS(hdc,0,0,"Filling buffer with code...");
	if (!FillInitialNOPCode())
		MessageBox(NULL,"Unable to initialize buffer with NOPs (test)","",MB_OK);

	TextOutS(hdc,0,0,"Executing code...                 ");
	if (!TestExecuteInitialNOPCode())
		MessageBox(NULL,"Unable to execute buffer with NOPs (test)","",MB_OK);

	TextOutS(hdc,0,0,"NOP DONE                              ");
	ReleaseDC(hwndMain,hdc);
}

void MSG16test() {
	HDC hdc;
	BYTE FAR *base,*p;
	DWORD code,MessageBox16_addr;
	unsigned int i;

#if TARGET_MSDOS == 32
	MessageBox16_addr = GetProcAddress16(win9x_user_win16,"MESSAGEBOX");
#else
	MessageBox16_addr = (DWORD)GetProcAddress(GetModuleHandle("USER"),"MESSAGEBOX");
#endif
	if (MessageBox16_addr == 0) {
		MessageBox(NULL,"Unable to get MessageBox entry point","",MB_OK);
		return;
	}

	hdc = GetDC(hwndMain);

	/* NTS: Don't worry, LockWin16EBbuffer() will not double-lock the buffer */
	TextOutS(hdc,0,0,"Locking buffer...");
	if ((base=p=LockWin16EBbuffer()) == NULL) {
		MessageBox(NULL,"Unable to lock buffer","",MB_OK);
		return;
	}

	/* force creation of the code alias */
	code = GetCodeAliasWin16EBbuffer();
	if (code == 0) {
		MessageBox(NULL,"Unable to get buffer code alias","",MB_OK);
		UnlockWin16EBbuffer();
		return;
	}

	/* write code that calls MessageBox() */
		/* MOV AX,<hwnd> */
		*p++ = 0xB8; *((WORD FAR*)p) = 0x0000; p+=2;
		/* PUSH AX */
		*p++ = 0x50;

		/* MOV AX,<lpszText> */
		*p++ = 0xB8; *((WORD FAR*)p) = Win16EBbufferdata16sel; p+=2; /* SEGMENT */
		/* PUSH AX */
		*p++ = 0x50;
		/* MOV AX,<lpszText> */
		*p++ = 0xB8; *((WORD FAR*)p) = 0x0080; p+=2;	/* OFFSET (0x80 in this buffer) */
		/* PUSH AX */
		*p++ = 0x50;

		/* MOV AX,<lpszTitle> */
		*p++ = 0xB8; *((WORD FAR*)p) = Win16EBbufferdata16sel; p+=2; /* SEGMENT */
		/* PUSH AX */
		*p++ = 0x50;
		/* MOV AX,<lpszText> */
		*p++ = 0xB8; *((WORD FAR*)p) = 0x00C0; p+=2;	/* OFFSET (0xC0 in this buffer) */
		/* PUSH AX */
		*p++ = 0x50;

		/* MOV AX,<fuStyle> */
		*p++ = 0xB8; *((WORD FAR*)p) = 0/*MB_OK*/|0x10/*MB_ICONHAND*/|0x1000/*MB_SYSTEMMODAL*/; p+=2;
		/* PUSH AX */
		*p++ = 0x50;

		/* CALL FAR [MessageBox] */
		*p++ = 0x9A;
		*((WORD FAR*)p) = MessageBox16_addr; p+=2;
		*((WORD FAR*)p) = MessageBox16_addr>>16; p+=2;

		/* RETF */
		*p++ = 0xCB;

		/* put the strings in place */
		{
			static const char *text = "Tada! System modal dialog!";
			for (i=0;text[i] != 0;i++) base[0x80+i] = text[i];
			base[0x80+i] = 0;
		}
		{
			static const char *text = "HAX HAX HAX";
			for (i=0;text[i] != 0;i++) base[0xC0+i] = text[i];
			base[0xC0+i] = 0;
		}

	/* run it! */
	ExecuteWin16EBbuffer(code);

	TextOutS(hdc,0,0,"NOP DONE                              ");
	ReleaseDC(hwndMain,hdc);
	UnlockWin16EBbuffer();
}

WindowProcType WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_TIMER) {
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
		if (wparam == '1') {
			NOPtest();
		}
		else if (wparam == '2') {
			MSG16test();
		}
		else if (wparam == 'B') {
			if (MessageBox(hwnd,"This will attempt to call INT 10h Set Video Mode, Continue?","WARNING!",MB_YESNO) == IDYES)
				INT10test();
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
			rct.top = 5+32+5;
			rct.left = 5;

			BeginPaint(hwnd,&ps);
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

	myInstance = hInstance;

	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	if (!AppIcon) {
		MessageBox(NULL,"Unable to load app icon","Oops!",MB_OK);
		return 1;
	}

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

	if (!InitWin16EB())
		MessageBox(NULL,"Unable to initialize Win16eb library","",MB_OK);

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	FreeWin16EB();
	return msg.wParam;
}

