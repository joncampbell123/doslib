#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif

/* Windows programming notes:
 *
 *   - If you're writing your software to work only on Windows 3.1 or later, you can omit the
 *     use of MakeProcInstance(). Windows 3.0 however require your code to use it.
 *
 *   - The Window procedure must be exported at link time. Windows 3.0 demands it.
 *
 *   - If you want your code to run in Windows 3.0, everything must be MOVEABLE. Your code and
 *     data segments must be MOVEABLE. If you intend to load resources, the resources must be
 *     MOVEABLE. The constraints of real mode and fitting everything into 640KB or less require
 *     it.
 *
 *   - If you want to keep your sanity, never mark your data segment (DGROUP) as discardable.
 *     You can make your code discardable because the mechanisms of calling your code by Windows
 *     including the MakeProcInstance()-generated wrapper for your windows proc will pull your
 *     code segment back in on demand. There is no documented way to pull your data segment back
 *     in if discarded. Notice all the programs in Windows 2.0/3.0 do the same thing.
 */
/* FIXME: This code crashes when multiple instances are involved. Especially the win386 build. */

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <windows/apihelp.h>

HWND near			hwndMain;
const char near			WndProcClass[] = "CLIPBRD1WINDOWS";
HINSTANCE near			myInstance;

HWND near			cbListHwnd = NULL;

static void DoClipboardSave(void) {
	MessageBox(hwndMain,"SAVE","TODO",MB_OK);
}

struct clipboard_fmt_t {
	UINT			cbFmt;
};

#define MAX_CLIPBOARD_FORMATS 256
static struct clipboard_fmt_t clipboard_format[MAX_CLIPBOARD_FORMATS];
static unsigned int clipboard_format_count = 0;

static void PopulateClipboardFormats(void) {
	char tmp[128],*w=tmp,*f=tmp+sizeof(tmp)-1;

	SendMessage(cbListHwnd,LB_RESETCONTENT,0,0);

	if (OpenClipboard(hwndMain)) {
		UINT efmt = 0,nfmt,i = 0;
		int r;

		while (i < MAX_CLIPBOARD_FORMATS) {
			nfmt = EnumClipboardFormats(efmt);
			if (nfmt == 0) break;
			efmt = nfmt;

			clipboard_format[i].cbFmt = efmt;

			w = tmp;
			switch (efmt) {
				case CF_BITMAP:
					strcpy(w,"CF_BITMAP"); break;
				case CF_DIB:
					strcpy(w,"CF_DIB"); break;
				case CF_DIF:
					strcpy(w,"CF_DIF"); break;
				case CF_DSPBITMAP:
					strcpy(w,"CF_DSPBITMAP"); break;
				case CF_DSPMETAFILEPICT:
					strcpy(w,"CF_DSPMETAFILEPICT"); break;
				case CF_DSPTEXT:
					strcpy(w,"CF_DSPTEXT"); break;
				case CF_METAFILEPICT:
					strcpy(w,"CF_METAFILEPICT"); break;
				case CF_OEMTEXT:
					strcpy(w,"CF_OEMTEXT"); break;
				case CF_OWNERDISPLAY:
					strcpy(w,"CF_OWNERDISPLAY"); break;
				case CF_PALETTE:
					strcpy(w,"CF_PALETTE"); break;
				case CF_PENDATA:
					strcpy(w,"CF_PENDATA"); break;
				case CF_RIFF:
					strcpy(w,"CF_RIFF"); break;
				case CF_SYLK:
					strcpy(w,"CF_SYLK"); break;
				case CF_TEXT:
					strcpy(w,"CF_TEXT"); break;
				case CF_TIFF:
					strcpy(w,"CF_TIFF"); break;
				case CF_WAVE:
					strcpy(w,"CF_WAVE"); break;
				default:
					if (efmt >= 0xC000/*registered*/) {
						r = GetClipboardFormatName(efmt,w,(int)(f-w));
						if (r < 0) r = 0;
						w += r; if (w > f) w = f;
						w += snprintf(w,(int)(f-w)," 0x%x",(unsigned)efmt);
						*w = 0;
					}
					else {
						w += snprintf(w,(int)(f-w),"??? 0x%x",(unsigned)efmt);
					}
					break;
			}

			SendMessage(cbListHwnd,LB_ADDSTRING,0,(LPARAM)tmp);
			i++;
		}

		clipboard_format_count = i;
		CloseClipboard();
	}
}

WindowProcType_NoLoadDS WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		cbListHwnd = CreateWindow("LISTBOX","",
			WS_CHILD | LBS_MULTIPLESEL | LBS_NOINTEGRALHEIGHT,/*style*/
			0,0,64,64,/*initial pos+size*/
			hwnd,/*parent window*/
			NULL,/*menu*/
			myInstance,
			NULL);
		ShowWindow(cbListHwnd,SHOW_OPENNOACTIVATE);
		PopulateClipboardFormats();

		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		if (cbListHwnd) DestroyWindow(cbListHwnd);
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
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else if (message == WM_COMMAND) {
		if (wparam == IDC_FILE_QUIT) {
			PostMessage(hwnd,WM_CLOSE,0,0);
		}
		else if (wparam == IDC_FILE_SAVE) {
			DoClipboardSave();
		}
		else if (wparam == IDC_FILE_REFRESH) {
			PopulateClipboardFormats();
		}
	}
	else if (message == WM_SIZE) {
#if WINVER >= 0x200 /* SetWindowPos() did not appear until Windows 2.x */
		unsigned int w = LOWORD(lparam),h = HIWORD(lparam);
		SetWindowPos(cbListHwnd,HWND_TOP,0,0,w,h,SWP_NOACTIVATE);
#endif
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

	/* NTS: In the Windows 3.1 environment all handles are global. Registering a class window twice won't work.
	 *      It's only under 95 and later (win32 environment) where Windows always sets hPrevInstance to 0
	 *      and window classes are per-application.
	 *
	 *      Windows 3.1 allows you to directly specify the FAR pointer. Windows 3.0 however demands you
	 *      MakeProcInstance it to create a 'thunk' so that Windows can call you (ick). */
	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = (WNDPROC)WndProc;
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
		wnd.hCursor = NULL;
		wnd.hbrBackground = NULL;
		wnd.lpszMenuName = MAKEINTRESOURCE(IDM_MAINMENU);
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
	}
	else {
#if defined(WIN386)
		/* FIXME: Win386 builds will CRASH if multiple instances try to run this way.
		 *        Somehow, creating a window with a class registered by another Win386 application
		 *        causes Windows to hang. */
		if (MessageBox(NULL,"Win386 builds may crash if you run multiple instances. Continue?","",MB_YESNO|MB_ICONEXCLAMATION|MB_DEFBUTTON2) == IDNO)
			return 1;
#endif
	}

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

	return msg.wParam;
}

