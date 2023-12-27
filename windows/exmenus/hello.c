#ifndef TARGET_WINDOWS
# error This is Windows code, not DOS
#endif
#ifndef TARGET_MSDOS
# error WTF
#endif

/* FIXME: Why does this program hard-crash Windows 3.0 Real Mode? */

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
 *
 *   - When running this program under Windows 3.1 in DOSBox, do NOT use core=normal and cputype=486_slow.
 *     Weird random errors show up and you will pull your hair out and waste entire weekends trying to debug
 *     them. In this program's case, COMDLG32.DLL will for whatever reason fail to initialize and Windows
 *     will show a "library failed to initialize" error. Set core=dynamic and cputype=pentium_slow, which
 *     seems to provide more correct emulation and allow Win32s to work properly.
 *
 *     This sort of problem doesn't exactly surprise me since DOSBox was designed to run...
 *     you know... *DOS games* anyway, so this doesn't exactly surprise me.
 */
/* Fun facts: The "Common dialog" routines didn't appear until Windows 3.1. If we blindly rely on GetOpenFileName
 *            the binary won't run under Windows 3.0. Closer examination of everything in Windows 3.0 makes it
 *            obvious that everyone rolled their own! */

#include <windows.h>
#if (TARGET_MSDOS == 16 && TARGET_WINDOWS >= 31) || TARGET_MSDOS == 32
# include <commdlg.h>
#endif
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <windows/apihelp.h>

HWND near		hwndMain;
const char near		WndProcClass[] = "HELLOWINDOWS";
const char near		HelloWorldText[] = "Hello world!";
const char near		openSaveFilter[] = "Text\0*.txt\0Executable\0*.exe\0Other\0*.*\0";
HINSTANCE near		myInstance;
HICON near		AppIcon;
HMENU near		AppMenu;
HCURSOR near		AppCursor;
HBITMAP near		AppBitmap;

void near AskFileOpen() {
#if (TARGET_MSDOS == 16 && TARGET_WINDOWS >= 31) || TARGET_MSDOS == 32
	OPENFILENAME f;
	char filename[129];

# if TARGET_MSDOS == 16
	_fmemset(&f,0,sizeof(f));
# else
	memset(&f,0,sizeof(f));
# endif
	f.lpstrFile = filename;
	f.nMaxFile = sizeof(filename);
	f.lStructSize = sizeof(f);
	f.hwndOwner = hwndMain;
	f.hInstance = myInstance;
	f.lpstrFilter = openSaveFilter;
	f.nFilterIndex = 1;
	f.lpstrTitle = "Open what?";
	f.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	filename[0] = 0;
	if (GetOpenFileName(&f)) {
		MessageBox(hwndMain,f.lpstrFile,"You selected",MB_OK);
	}
#else
	MessageBox(hwndMain,"GetOpenFileName didn't appear until Windows 3.1","Oops!",MB_OK);
#endif
}

void near AskFileSave() {
#if (TARGET_MSDOS == 16 && TARGET_WINDOWS >= 31) || TARGET_MSDOS == 32
	OPENFILENAME f;
	char filename[129];

# if TARGET_MSDOS == 16
	_fmemset(&f,0,sizeof(f));
# else
	memset(&f,0,sizeof(f));
# endif
	f.lpstrFile = filename;
	f.nMaxFile = sizeof(filename);
	f.lStructSize = sizeof(f);
	f.hwndOwner = hwndMain;
	f.hInstance = myInstance;
	f.lpstrFilter = openSaveFilter;
	f.nFilterIndex = 1;
	f.lpstrTitle = "Save to what?";
	f.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	filename[0] = 0;
	if (GetSaveFileName(&f)) {
		MessageBox(hwndMain,f.lpstrFile,"You selected",MB_OK);
	}
#else
	MessageBox(hwndMain,"GetSaveFileName didn't appear until Windows 3.1","Oops!",MB_OK);
#endif
}

#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
FARPROC HelpAboutProc_MPI;
#endif
DialogProcType_NoLoadDS HelpAboutProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_INITDIALOG) {
		SetFocus(GetDlgItem(hwnd,IDOK));
		return 1; /* Success */
	}
	else if (message == WM_COMMAND) {
		if (HIWORD(lparam) == 0) { /* from a menu */
			switch (LOWORD(wparam)) { /* NTS: For Win16: the whole "WORD", for Win32, the lower 16 bits */
				case IDOK:
					EndDialog(hwnd,0);
					break;
				case IDCANCEL:
					EndDialog(hwnd,0);
					break;
			};
		}
	}

	return 0;
}

WindowProcType_NoLoadDS WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(AppCursor);
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_MOUSEMOVE) {
#if WINVER < 0x200
		SetCursor(AppCursor);
		return 1;
#else
		return DefWindowProc(hwnd,message,wparam,lparam);
#endif
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

			BeginPaint(hwnd,&ps);
			TextOut(ps.hdc,0,0,HelloWorldText,strlen(HelloWorldText));
			if (AppIcon) DrawIcon(ps.hdc,5,20,AppIcon);
			if (AppBitmap) {
				HBITMAP oldBitmap;
				HDC blitDC;

				blitDC = CreateCompatibleDC(ps.hdc);
				oldBitmap = (HBITMAP)SelectObject(blitDC,(HGDIOBJ)AppBitmap);
				BitBlt(ps.hdc,5,20+32+4,300,100,blitDC,0,0,SRCCOPY);
				SelectObject(blitDC,(HGDIOBJ)oldBitmap);
				DeleteDC(blitDC);
			}
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else if (message == WM_COMMAND) {
		if (HIWORD(lparam) == 0) { /* from a menu */
			switch (LOWORD(wparam)) {
				case IDC_FILE_OPEN:
					AskFileOpen();
					break;
				case IDC_FILE_SAVE:
					AskFileSave();
					break;
				case IDC_FILE_QUIT:
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDC_HELP_ABOUT:
#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
					DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwnd,HelpAboutProc_MPI);
#else
					DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwnd,HelpAboutProc);
#endif
					break;
			};
		}
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

	AppMenu = LoadMenu(hInstance,MAKEINTRESOURCE(IDM_MAINMENU));
	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	AppCursor = LoadCursor(hInstance,MAKEINTRESOURCE(IDC_HELLOCURSOR));
	AppBitmap = LoadBitmap(hInstance,MAKEINTRESOURCE(IDB_BLISS));

#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
	HelpAboutProc_MPI = MakeProcInstance((FARPROC)HelpAboutProc,hInstance);
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
		wnd.lpfnWndProc = (WNDPROC)MakeProcInstance((FARPROC)WndProc,hInstance);
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

	hwndMain = CreateWindow(WndProcClass,"Hello!",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		600,240,
		NULL,NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	if (AppMenu) SetMenu(hwndMain,AppMenu);
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

