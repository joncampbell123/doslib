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
 */
/* Fun facts: The "Common dialog" routines didn't appear until Windows 3.1. If we blindly rely on GetOpenFileName
 *            the binary won't run under Windows 3.0. Closer examination of everything in Windows 3.0 makes it
 *            obvious that everyone rolled their own! */

#include <windows.h>
#if (TARGET_MSDOS == 16 && TARGET_WINDOWS >= 31) || TARGET_MSDOS == 32
# include <commdlg.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <windows/apihelp.h>

HWND near		hwndMain;
const char near		WndProcClass[] = "HELLOWINDOWS";
const char near		HelloWorldText[] = "Hello world! I can mess with the System Menu too!";
const char near		openSaveFilter[] = "Text\0*.txt\0Executable\0*.exe\0Other\0*.*\0";
const char near		str_times[] = "times";
const char near		str_time[] = "time";
HINSTANCE near		myInstance;
HMENU near		SysMenu;
HICON near		AppIcon;
HMENU near		AppMenu;
HCURSOR near		AppCursor;
HBITMAP near		AppBitmap;
HMENU near		FileMenu,HelpMenu;
int near		SysMenuInitCount = 0;
BOOL near		AllowWindowMove = TRUE;

// Open Watcom sprintf() cannot print %u integers properly in real mode,
// and makes calls to KERNEL IsLeadDBCSByte() which does not exit in Windows 1.0
static void near intToStr(char *d,unsigned int dmax,int x) {
	if (dmax > 1) {
		if (x < 0) {
			*d++ = '-';
			dmax--;
			x = -x;
		}
	}

	if (x != 0) {
		unsigned int tmpsi=0;
		char tmps[16];

		while (x != 0) {
			tmps[tmpsi++] = (char)(((unsigned int)x % 10u) + '0');
			x = (int)((unsigned int)x / 10u);
		}
		if (tmpsi == 0) tmps[tmpsi++] = '0';

		while (dmax > 1 && tmpsi > 0) {
			*d++ = tmps[--tmpsi];
			dmax--;
		}
	}

	if (dmax > 0) {
		*d++ = 0;
		dmax--;
	}
}

static char * near strcpy_and_advance(char *d,const char *s) {
	while (*s != 0) *d++ = *s++;
	return d;
}

#if (WINVER < 0x300)
/* AppendMenu() did not exist until Windows 3.0, this is a polyfill */
static BOOL near AppendMenu(HMENU hMenu,UINT fuFlags,UINT idNewItem,LPCSTR lpNewItem) {
	/* Microsoft's Windows 3.1 SDK barely documents this function at all, because you're "supposed" to use the newer functions.
	 * Also back in the Windows 1.x and 2.x SDK days Microsoft declared the functions in their headers without parameter names. */
	return ChangeMenu(hMenu,0/*position not used*/,lpNewItem,idNewItem,MF_APPEND | fuFlags);
}

static BOOL near ModifyMenu(HMENU hMenu,UINT idItem,UINT fuFlags,UINT idNewItem,LPCSTR lpNewItem) {
	/* Microsoft's Windows 3.1 SDK has forgotten about MF_CHANGE and does not mention it */
	return ChangeMenu(hMenu,idItem,lpNewItem,idNewItem,MF_CHANGE | fuFlags);
}
#endif

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
#if WINVER < 0x200 /* WM_SETCUROR doesn't exist in Windows 1.0 */
		SetCursor(AppCursor);
#endif
		return 1;
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
	/* this example demonstrates the WM_INITMENU and WM_INITMENUPOPUP messages sent by Windows
	 * when a menu is displayed. In this case, we respond to the message that the System Menu
	 * of our window is being display and use that to count the number of times it has been accessed */
	else if (message == WM_INITMENUPOPUP) { /* when a menu is first accessed */
		if ((HMENU)wparam == SysMenu) {
			{
				char *tmp = malloc(128);
				char *tmp2 = malloc(24);
				if (tmp && tmp2) {
					char *w = tmp;

					intToStr(tmp2,sizeof(tmp2),++SysMenuInitCount); // Open Watcom sprintf() does not work properly with %u in real mode

					w = strcpy_and_advance(w,"This menu called ");
					w = strcpy_and_advance(w,tmp2);
					*w++ = ' ';
					w = strcpy_and_advance(w,SysMenuInitCount > 1 ? str_times : str_time);
					*w = 0;

					ModifyMenu(SysMenu,IDC_SYS_COUNTER,MF_BYCOMMAND|MF_STRING|MF_DISABLED,IDC_SYS_COUNTER,tmp);
				}
				if (tmp2) free(tmp2);
				if (tmp) free(tmp);
			}

			/* we also want our AllowWindowMove status to be reflected in the System Menu */
			EnableMenuItem(SysMenu,SC_MOVE,MF_BYCOMMAND|
				(AllowWindowMove ? MF_ENABLED : (MF_DISABLED|MF_GRAYED)));
		}

		return DefWindowProc(hwnd,message,wparam,lparam);
	}
	else if (message == WM_SYSCOMMAND) { /* System menu item was selected */
		if (LOWORD(wparam) >= 0xF000) {
			/* "In WM_SYSCOMMAND messages the four low-order bits of the wCmdType (wparam) parameter
			 * are used internally by Windows. When an application tests the value of wCmdType it must
			 * combine the value 0xFFF0 with wCmdType to obtain the correct result"
			 *
			 * OK whatever Microsoft */
			switch (LOWORD(wparam) & 0xFFF0) {
				case SC_MOVE:
					if (AllowWindowMove) 
						return DefWindowProc(hwnd,message,wparam,lparam);
					break;
				case SC_MAXIMIZE:	/* show that we can intercept and ignore certain system menu commands */
					/* when the user selects "maximize", or clicks the maximize button in the non-client area,
					 * or double-clicks the title bar, Windows sends WM_SYSCOMMAND with SC_MAXIMIZE. we
					 * ignore the command and show this message instead. note that this doesn't prevent other
					 * programs or ourselves from using other means to maximize like ShowWindow() */
					MessageBox(hwnd,"Uhm......... No thanks :)","Smart-ass response",MB_OK);
					break;
				default:
					return DefWindowProc(hwnd,message,wparam,lparam);
			}
		}
		else {
			switch (LOWORD(wparam)) {
				case IDC_HELP_ABOUT:
#ifdef WIN16_NEEDS_MAKEPROCINSTANCE
					DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwnd,HelpAboutProc_MPI);
#else
					DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwnd,HelpAboutProc);
#endif
					break;
			}
		}
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
				case IDC_FILE_WINDOWMOVE:
					AllowWindowMove = !AllowWindowMove;
					CheckMenuItem(FileMenu,IDC_FILE_WINDOWMOVE,MF_BYCOMMAND|(AllowWindowMove?MF_CHECKED:MF_UNCHECKED));
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

	AppMenu = CreateMenu();
	if (!AppMenu) {
		return 1;
	}

	FileMenu = CreateMenu();
	if (!FileMenu) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_ENABLED,IDC_FILE_OPEN,"&Open")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_ENABLED,IDC_FILE_SAVE,"&Save")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_SEPARATOR,0,"")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_ENABLED|MF_CHECKED,IDC_FILE_WINDOWMOVE,"Allow &moving the window")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_SEPARATOR,0,"")) {
		return 1;
	}

	if (!AppendMenu(FileMenu,MF_STRING|MF_ENABLED,IDC_FILE_QUIT,"&Quit")) {
		return 1;
	}

	HelpMenu = CreateMenu();
	if (!HelpMenu) {
		return 1;
	}

	if (!AppendMenu(HelpMenu,MF_STRING|MF_ENABLED,IDC_HELP_ABOUT,"&About")) {
		return 1;
	}

	if (!AppendMenu(AppMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)FileMenu,"&File")) {
		return 1;
	}

	if (!AppendMenu(AppMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)HelpMenu,"&Help")) {
		return 1;
	}

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

	SysMenu = GetSystemMenu(hwndMain,FALSE); /* get system menu, make a copy, return it */
	if (SysMenu == NULL) {
		MessageBox(NULL,"Unable to retrieve system menu","Oops!",MB_OK);
		return 1;
	}

	AppendMenu(SysMenu,MF_SEPARATOR,0,"");
	AppendMenu(SysMenu,MF_STRING,IDC_HELP_ABOUT,"About this program..."); /* NTS: Any ID is OK as long at it's less than 0xF000 */
	AppendMenu(SysMenu,MF_SEPARATOR,0,"");
	AppendMenu(SysMenu,MF_STRING|MF_DISABLED,IDC_SYS_COUNTER,"This menu hasn't been accessed yet");

	SetMenu(hwndMain,AppMenu);
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

	/* NTS: Do not DestroyMenu(). Windows will do that for us on destruction of the window.
	 *      If we do, Win16 systems will act funny, ESPECIALLY Windows 3.0 */
	return msg.wParam;
}

