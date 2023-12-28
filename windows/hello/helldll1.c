#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

/* FIXME: Why can't Windows 3.0 Real Mode load our DLL? It complains about "Insufficient memory" */

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
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <windows/apihelp.h>

HWND		hwndMain;
const char*	WndProcClass = "HELLOWINDOWS";
HINSTANCE	myInstance;
HICON		AppIcon;

/* symbols to import from DLL */
void DllEntryType HelloMsg();
void DllEntryType HelloDraw(HWND hwnd,HDC hdc,HICON icon);

#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
FARPROC WndProc_MPI;
#endif
WindowProcType WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0; /* OK */
	}
#ifdef WM_SETCURSOR
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
#endif
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

			BeginPaint(hwnd,&ps);
			HelloDraw(hwnd,ps.hdc,AppIcon);
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

#if TARGET_MSDOS == 16 && TARGET_WINDOWS < 31 /* Windows 3.0 or older (any version capable of real-mode) */
    /* our data segment is discardable.
     * in Windows 3.0 real mode that means our data segment can disappear out from under us.
     * unfortunately I don't know how to call the data segment back in.
     * so we have to compensate by locking our data segment in place. */
    LockData();
#endif

	/* FIXME: Windows 3.0 Real Mode: Why are we unable to load our own Application Icon? */
	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	if (!AppIcon) MessageBox(NULL,"Unable to load app icon","Oops!",MB_OK);

#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
	WndProc_MPI = MakeProcInstance((FARPROC)WndProc,hInstance);
#endif

	/* NTS: In the Windows 3.1 environment all handles are global. Registering a class window twice won't work.
	 *      It's only under 95 and later (win32 environment) where Windows always sets hPrevInstance to 0
	 *      and window classes are per-application.
	 *
	 *      Windows 3.1 allows you to directly specify the FAR pointer. Windows 3.0 however demands you
	 *      MakeProcInstance it to create a 'thunk' so that Windows can call you (ick). */
	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
		wnd.lpfnWndProc = (WNDPROC)WndProc_MPI;
#else
		wnd.lpfnWndProc = WndProc;
#endif
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

	HelloMsg();

	hwndMain = CreateWindow(WndProcClass,"Hello!",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		300,200,
		NULL,NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain); /* FIXME: For some reason this only causes WM_PAINT to print gibberish and cause a GPF. Why? And apparently, Windows 3.0 repaints our window anyway! */

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#if TARGET_MSDOS == 16
    /* Win16 only:
     * If we are the owner (the first instance that registered the window class),
     * then we must reside in memory until we are the last instance resident.
     * If we do not do this, then if multiple instances are open and the user closes US
     * before closing the others, the others will crash (having pulled the code segment
     * behind the window class out from the other processes). */
	if (!hPrevInstance) {
        while (GetModuleUsage(hInstance) > 1) {
            PeekMessage(&msg,NULL,0,0,PM_REMOVE);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
#endif

	return msg.wParam;
}

