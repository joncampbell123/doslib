/* winfcon.c
 *
 * Fake console for Windows applications where a console is not available.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * This code allows the DOS/CPU test code to print to a console despite the
 * fact that Windows 3.0/3.1 and Win32s do not provide a console. For this
 * code to work, the program must include specific headers and #define a
 * macro. The header will then redefine various standard C functions to
 * redirect their use into this program. This code is not used for targets
 * that provide a console.
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
# include <windows/apihelp.h>
#else
# error what
#endif

#include <setjmp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/doswin.h>
#include <hw/dos/winfcon.h>

#ifdef WIN_STDOUT_CONSOLE

/* _export is not valid for Win32. this silences a Watcom linker warning */
#if TARGET_MSDOS == 32
# define winproc_export
#else
# define winproc_export _export
#endif

#undef read
#undef write
#undef getch
#undef isatty

#define KBSIZE		256

static char		_win_WindowProcClass[128];

/* If we stick all these variables in the data segment and reference
 * them directly, then we'll work from a single instance, but run into
 * problems with who's data segment to use once we run in multiple
 * instances. The problem, is that when an application creates a
 * window of our class, the Window Proc is not guaranteed to be called
 * with our DS segment/selector. In fact, it often seems to be the
 * data segment of the first instance by which the window class was
 * created. And under Windows 3.0, unless you used __loadds and
 * MakeProcInstance, the DS segment could be any random segment
 * that happened to be there when you were called.
 *
 * Our Window Proc below absolves itself of these problems by storing
 * the pointer to the context in the Window data associated with the
 * window (GetWindowLong/SetWindowLong), then using only that context
 * pointer for maintaining itself.
 *
 * This DS segment limitation only affects the Window procedure and
 * any functions called by the Window procedure. Other parts, like
 * WinMain, do not have to worry about whether DS is correct and the
 * data segment will always be the current instance running.
 *
 * Note that the above limitations are only an issue for the Win16
 * world. The Win32 world is free from this hell and so we only
 * have to maintain one context structure. */
typedef struct _win_console_ctx {
	char		console[80*25];
	char		_win_kb[KBSIZE];
	int		conHeight,conWidth;
	int		_win_kb_i,_win_kb_o;
	int		monoSpaceFontHeight;
#if TARGET_MSDOS == 32 && defined(WIN386)
	short int	monoSpaceFontWidth;
#else
	int		monoSpaceFontWidth;
#endif
	HFONT		monoSpaceFont;
	int		pendingSigInt;
	int		userReqClose;
	int		allowClose;
	int		conX,conY;
	jmp_buf		exit_jmp;
	HWND		hwndMain;
	int		myCaret;
#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
	WORD		my_ds;
#endif	
};

HINSTANCE			_win_hInstance;
static struct _win_console_ctx	_this_console;
static char			temprintf[1024];

#if TARGET_MSDOS == 32 && defined(WIN386)
# define USER_GWW_CTX			0
# define USER_GWW_MAX			6
#elif TARGET_MSDOS == 16
# define USER_GWW_CTX			0
# define USER_GWW_MAX			4
#else
# define USER_GWW_MAX			0
#endif
#define USER_GCW_MAX			0

HWND _win_hwnd() {
	return _this_console.hwndMain;
}

int _win_kb_insert(struct _win_console_ctx FAR *ctx,char c) {
	if ((ctx->_win_kb_i+1)%KBSIZE == ctx->_win_kb_o) {
		MessageBeep(-1);
		return -1;
	}

	ctx->_win_kb[ctx->_win_kb_i] = c;
	if (++ctx->_win_kb_i >= KBSIZE) ctx->_win_kb_i = 0;
	return 0;
}

void _win_sigint() {
	void (*sig)(int x) = signal(SIGINT,SIG_DFL);
	if (sig != SIG_IGN && sig != SIG_DFL) sig(SIGINT);
	signal(SIGINT,sig);
	if (sig == SIG_DFL) longjmp(_this_console.exit_jmp,1);
}

void _win_sigint_post(struct _win_console_ctx FAR *ctx) {
	/* because doing a longjmp() out of a Window proc is very foolish */
	ctx->pendingSigInt = 1;
}

#if ((TARGET_MSDOS == 16 && TARGET_WINDOWS < 31) || (TARGET_MSDOS == 32 && defined(WIN386)))
FARPROC _win_WindowProc_MPI;
#endif
/* NTS: Win16 only: DS (data segment) is NOT necessarily the data segment of the instance
 *      that spawned the window! Any attempt to access local variables will likely refer
 *      to the copy in the first instance */
/* NTS: All code in this routine deliberately does not refer to the local data segment, unless it
 *      has to (which it does through the segment value in the context). This reduces problems with
 *      the screwy callback design in Windows 3.0/3.1. */
/* NTS: Do NOT use __loadds on this function prototype. It will seem to work, but because __loadds
 *      reloads the (cached) instance data segment it will cause all instances to crash when you
 *      spawn multiple instances and then close the first one you spawned. NOT using __loadds
 *      removes that crash. */
WindowProcType_NoLoadDS winproc_export _win_WindowProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#if TARGET_MSDOS == 32 && defined(WIN386)
	struct _win_console_ctx FAR *_ctx_console;
	{
		unsigned short s = GetWindowWord(hwnd,USER_GWW_CTX);
		unsigned int o = GetWindowLong(hwnd,USER_GWW_CTX+2);
		_ctx_console = (void far *)MK_FP(s,o);
	}
	if (_ctx_console == NULL) return DefWindowProc(hwnd,message,wparam,lparam);
#elif TARGET_MSDOS == 16
	struct _win_console_ctx FAR *_ctx_console;
	_ctx_console = (void far *)GetWindowLong(hwnd,USER_GWW_CTX);
	if (_ctx_console == NULL) return DefWindowProc(hwnd,message,wparam,lparam);
#else
# define _ctx_console (&_this_console)
#endif

	if (message == WM_GETMINMAXINFO) {
#if TARGET_MSDOS == 32 && defined(WIN386) /* Watcom Win386 does NOT translate LPARAM for us */
		MINMAXINFO FAR *mm = (MINMAXINFO FAR*)win386_help_MapAliasToFlat(lparam);
		if (mm == NULL) return DefWindowProc(hwnd,message,wparam,lparam);
#else
		MINMAXINFO FAR *mm = (MINMAXINFO FAR*)(lparam);
#endif
		mm->ptMaxSize.x = (_ctx_console->monoSpaceFontWidth * _ctx_console->conWidth) +
			(2 * GetSystemMetrics(SM_CXFRAME));
		mm->ptMaxSize.y = (_ctx_console->monoSpaceFontHeight * _ctx_console->conHeight) +
			(2 * GetSystemMetrics(SM_CYFRAME)) + GetSystemMetrics(SM_CYCAPTION);
		mm->ptMinTrackSize.x = mm->ptMaxSize.x;
		mm->ptMinTrackSize.y = mm->ptMaxSize.y;
		mm->ptMaxTrackSize.x = mm->ptMaxSize.x;
		mm->ptMaxTrackSize.y = mm->ptMaxSize.y;
		return 0;
	}
	else if (message == WM_CLOSE) {
		if (_ctx_console->allowClose) DestroyWindow(hwnd);
		else _win_sigint_post(_ctx_console);
		_ctx_console->userReqClose = 1;
	}
	else if (message == WM_DESTROY) {
		_ctx_console->allowClose = 1;
		_ctx_console->userReqClose = 1;
		if (_ctx_console->myCaret) {
			HideCaret(hwnd);
			DestroyCaret();
			_ctx_console->myCaret = 0;
		}

		PostQuitMessage(0);
		_ctx_console->hwndMain = NULL;
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
	else if (message == WM_ACTIVATE) {
		if (wparam) {
			if (!_ctx_console->myCaret) {
				CreateCaret(hwnd,NULL,_ctx_console->monoSpaceFontWidth,_ctx_console->monoSpaceFontHeight);
				SetCaretPos(_ctx_console->conX * _ctx_console->monoSpaceFontWidth,
					_ctx_console->conY * _ctx_console->monoSpaceFontHeight);
				ShowCaret(hwnd);
				_ctx_console->myCaret = 1;
			}
		}
		else {
			if (_ctx_console->myCaret) {
				HideCaret(hwnd);
				DestroyCaret();
				_ctx_console->myCaret = 0;
			}
		}

		/* BUGFIX: Microsoft Windows 3.1 SDK says "return 0 if we processed the message".
		 *         Yet if we actually do, we get funny behavior. Like if I minimize another
		 *         application's window and then activate this app again, every keypress
		 *         causes Windows to send WM_SYSKEYDOWN/WM_SYSKEYUP. Somehow passing it
		 *         down to DefWindowProc() solves this. */
		return DefWindowProc(hwnd,message,wparam,lparam);
	}
	else if (message == WM_CHAR) {
		if (wparam > 0 && wparam <= 126) {
			if (wparam == 3) {
				/* CTRL+C */
				if (_ctx_console->allowClose) DestroyWindow(hwnd);
				else _win_sigint_post(_ctx_console);
			}
			else {
				_win_kb_insert(_ctx_console,(char)wparam);
			}
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
			HFONT of;
			int y;

			BeginPaint(hwnd,&ps);
			SetBkMode(ps.hdc,OPAQUE);
			SetBkColor(ps.hdc,RGB(255,255,255));
			SetTextColor(ps.hdc,RGB(0,0,0));
			of = (HFONT)SelectObject(ps.hdc,_ctx_console->monoSpaceFont);
			for (y=0;y < _ctx_console->conHeight;y++) {
				TextOut(ps.hdc,0,y * _ctx_console->monoSpaceFontHeight,
					_ctx_console->console + (_ctx_console->conWidth * y),
					_ctx_console->conWidth);
			}
			SelectObject(ps.hdc,of);
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

int _win_kbhit() {
	_win_pump();
	return _this_console._win_kb_i != _this_console._win_kb_o;
}

int _win_getch() {
	do {
		if (_win_kbhit()) {
			int c = (int)((unsigned char)_this_console._win_kb[_this_console._win_kb_o]);
			if (++_this_console._win_kb_o >= KBSIZE) _this_console._win_kb_o = 0;
			return c;
		}

		_win_pump_wait();
	} while (1);

	return -1;
}

int _win_kb_read(char *p,int sz) {
	int cnt=0;

	while (sz-- > 0)
		*p++ = _win_getch();

	return cnt;
}

int _win_kb_write(const char *p,int sz) {
	int cnt=0;

	while (sz-- > 0)
		_win_putc(*p++);

	return cnt;
}

int _win_read(int fd,void *buf,int sz) {
	if (fd == 0) return _win_kb_read((char*)buf,sz);
	else if (fd == 1 || fd == 2) return -1;
	else return read(fd,buf,sz);
}

int _win_write(int fd,const void *buf,int sz) {
	if (fd == 0) return -1;
	else if (fd == 1 || fd == 2) return _win_kb_write(buf,sz);
	else return write(fd,buf,sz);
}

int _win_isatty(int fd) {
	if (fd == 0 || fd == 1 || fd == 2) return 1; /* we're emulating one, so, yeah */
	return isatty(fd);
}

void _win_pump_wait() {
	MSG msg;

	if (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		while (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	if (_this_console.pendingSigInt) {
		_this_console.pendingSigInt = 0;
		_win_sigint();
	}
}

void _win_pump() {
	MSG msg;

#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
	/* Hack: Windows has this nice "GetTickCount()" function that has serious problems
	 *       maintaining a count if we don't process the message pump! Doing this
	 *       prevents portions of this code from getting stuck in infinite loops
	 *       waiting for the damn timer to advance. Note that this is a serious
	 *       problem that only plagues Windows 3.1 and earlier. Windows 95 doesn't
	 *       have this problem.  */
	PostMessage(_this_console.hwndMain,WM_USER,0,0);
#endif
	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
		do {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} while (PeekMessage(&msg,NULL,0,0,PM_REMOVE));
	}

	if (_this_console.pendingSigInt) {
		_this_console.pendingSigInt = 0;
		_win_sigint();
	}
}

void _win_update_cursor() {
	if (_this_console.myCaret)
		SetCaretPos(_this_console.conX * _this_console.monoSpaceFontWidth,
			_this_console.conY * _this_console.monoSpaceFontHeight);
}

void _win_redraw_line_row() {
	if (_this_console.conY >= 0 && _this_console.conY < _this_console.conHeight) {
		HDC hdc = GetDC(_this_console.hwndMain);
		HFONT of;

		SetBkMode(hdc,OPAQUE);
		SetBkColor(hdc,RGB(255,255,255));
		SetTextColor(hdc,RGB(0,0,0));
		of = (HFONT)SelectObject(hdc,_this_console.monoSpaceFont);
		if (_this_console.myCaret) HideCaret(_this_console.hwndMain);
		TextOut(hdc,0,_this_console.conY * _this_console.monoSpaceFontHeight,
			_this_console.console + (_this_console.conWidth * _this_console.conY),_this_console.conWidth);
		if (_this_console.myCaret) ShowCaret(_this_console.hwndMain);
		SelectObject(hdc,of);
		ReleaseDC(_this_console.hwndMain,hdc);
	}
}

void _win_redraw_line_row_partial(int x1,int x2) {
	if (x1 >= x2) return;

	if (_this_console.conY >= 0 && _this_console.conY < _this_console.conHeight) {
		HDC hdc = GetDC(_this_console.hwndMain);
		HFONT of;

		SetBkMode(hdc,OPAQUE);
		SetBkColor(hdc,RGB(255,255,255));
		SetTextColor(hdc,RGB(0,0,0));
		of = (HFONT)SelectObject(hdc,_this_console.monoSpaceFont);
		if (_this_console.myCaret) HideCaret(_this_console.hwndMain);
		TextOut(hdc,x1 * _this_console.monoSpaceFontWidth,_this_console.conY * _this_console.monoSpaceFontHeight,
			_this_console.console + (_this_console.conWidth * _this_console.conY) + x1,x2 - x1);
		if (_this_console.myCaret) ShowCaret(_this_console.hwndMain);
		SelectObject(hdc,of);
		ReleaseDC(_this_console.hwndMain,hdc);
	}
}

void _win_scrollup() {
	HDC hdc = GetDC(_this_console.hwndMain);
	if (_this_console.myCaret) HideCaret(_this_console.hwndMain);
	BitBlt(hdc,0,0,_this_console.conWidth * _this_console.monoSpaceFontWidth,
		_this_console.conHeight * _this_console.monoSpaceFontHeight,hdc,
		0,_this_console.monoSpaceFontHeight,SRCCOPY);
	if (_this_console.myCaret) ShowCaret(_this_console.hwndMain);
	ReleaseDC(_this_console.hwndMain,hdc);

	memmove(_this_console.console,_this_console.console+_this_console.conWidth,
		(_this_console.conHeight-1)*_this_console.conWidth);
	memset(_this_console.console+((_this_console.conHeight-1)*_this_console.conWidth),
		' ',_this_console.conWidth);
}

void _win_newline() {
	_this_console.conX = 0;
	if ((_this_console.conY+1) == _this_console.conHeight) {
		_win_redraw_line_row();
		_win_scrollup();
		_win_redraw_line_row();
		_win_update_cursor();
		_gdi_pause();
	}
	else {
		_win_redraw_line_row();
		_this_console.conY++;
	}
}

/* write to the console. does NOT redraw the screen unless we get a newline or we need to scroll up */
void _win_putc(char c) {
	if (c == 10) {
		_win_newline();
	}
	else if (c == 13) {
		_this_console.conX = 0;
		_win_redraw_line_row();
		_win_update_cursor();
		_gdi_pause();
	}
	else {
		if (_this_console.conX < _this_console.conWidth)
			_this_console.console[(_this_console.conY*_this_console.conWidth)+_this_console.conX] = c;
		if (++_this_console.conX == _this_console.conWidth)
			_win_newline();
	}
}

size_t _win_printf(const char *fmt,...) {
	int fX = _this_console.conX;
	va_list va;
	char *t;

	va_start(va,fmt);
	vsnprintf(temprintf,sizeof(temprintf)-1,fmt,va);
	va_end(va);

	t = temprintf;
	if (*t != 0) {
		while (*t != 0) {
			if (*t == 13 || *t == 10) fX = 0;
			_win_putc(*t++);
		}
		if (fX <= _this_console.conX) _win_redraw_line_row_partial(fX,_this_console.conX);
		_win_update_cursor();
	}

	_win_pump();
	return 0;
}

/* HACK: I don't know if real systems do this or QEMU is doing something wrong, but apparently if a program
 *       rapidly prints a lot of text under Windows 3.1 (like the RDTSC test program) it can cause the GDI
 *       to become 100% focused on TextOut() to the point not even the cursor updates when you move it, and
 *       keyboard events to become severely stalled. Our solution to this problem is to see if we're running
 *       under Windows 3.1 or earlier, and if so, purposely slow down our output with a software delay */
void _gdi_pause() {
#if defined(TARGET_WINDOWS)
# if TARGET_MSDOS == 32 && defined(WIN386)
# else
	const DWORD ConDelay = 16UL; /* 16ms */
# endif
#endif

#if defined(TARGET_WINDOWS)
	if (windows_mode != WINDOWS_NT) {
# if TARGET_MSDOS == 32 && defined(WIN386)
		/* nothing */
# else
#  if TARGET_MSDOS == 16
		if (ToolHelpInit()) {
			DWORD base,m;
			TIMERINFO ti;
			ti.dwSize = sizeof(ti);
			if (__TimerCount(&ti)) {
				base = ti.dwmsSinceStart;

				do {
					Yield();
					_win_pump();
					if (!__TimerCount(&ti)) break;
					m = ti.dwmsSinceStart;
				} while ((m - base) < ConDelay);
			}
		}
		else {
#  else
		{
#  endif
			DWORD base,m;

			base = GetTickCount();
			do {
				Yield();
				_win_pump();
				m = GetTickCount();
			} while ((m - base) < ConDelay);
		}
# endif
	}
#endif
}

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL _win_main_con_entry(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow,int (_cdecl *_main_f)(int argc,char**,char**)) {
	WNDCLASS wnd;
	MSG msg;

	_win_hInstance = hInstance;
	snprintf(_win_WindowProcClass,sizeof(_win_WindowProcClass),"_HW_DOS_WINFCON_%lX",(DWORD)hInstance);
#if ((TARGET_MSDOS == 16 && TARGET_WINDOWS < 31) || (TARGET_MSDOS == 32 && defined(WIN386)))
	_win_WindowProc_MPI = MakeProcInstance((FARPROC)_win_WindowProc,hInstance);
#endif

	/* clear the console */
	memset(&_this_console,0,sizeof(_this_console));
	memset(_this_console.console,' ',sizeof(_this_console.console));
	_this_console._win_kb_i = _this_console._win_kb_o = 0;
	_this_console.conHeight = 25;
	_this_console.conWidth = 80;
	_this_console.conX = 0;
	_this_console.conY = 0;
#if TARGET_MSDOS == 16 || (TARGET_MSDOS == 32 && defined(WIN386))
	{
		WORD s=0;

		__asm {
			mov	ax,ds
			mov	s,ax
		}
		_this_console.my_ds = s;
	}
#endif

	/* we need to know at this point what DOS/Windows combination we're running under */
	probe_dos();
	detect_windows();

	/* we want each instance to have it's own WNDCLASS, even though Windows (Win16) considers them all instances
	 * coming from the same HMODULE. In Win32, there is no such thing as a "previous instance" anyway */
	wnd.style = CS_HREDRAW|CS_VREDRAW;
#if ((TARGET_MSDOS == 16 && TARGET_WINDOWS < 31) || (TARGET_MSDOS == 32 && defined(WIN386)))
	wnd.lpfnWndProc = (WNDPROC)_win_WindowProc_MPI;
#else
	wnd.lpfnWndProc = _win_WindowProc;
#endif
	wnd.cbClsExtra = USER_GCW_MAX;
	wnd.cbWndExtra = USER_GWW_MAX;
	wnd.hInstance = hInstance;
	wnd.hIcon = NULL;
	wnd.hCursor = NULL;
	wnd.hbrBackground = NULL;
	wnd.lpszMenuName = NULL;
	wnd.lpszClassName = _win_WindowProcClass;

	if (!RegisterClass(&wnd)) {
		MessageBox(NULL,"Unable to register Window class","Oops!",MB_OK);
		return 1;
	}

/* Use the full path of our EXE image by default */
	{
		char title[256];

		if (!GetModuleFileName(hInstance,title,sizeof(title)-1))
			strcpy(title,"<unknown>");

		_this_console.hwndMain = CreateWindow(_win_WindowProcClass,title,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,CW_USEDEFAULT,
			100,40,
			NULL,NULL,
			hInstance,NULL);
	}

	if (!_this_console.hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

#if TARGET_MSDOS == 32 && defined(WIN386)
	/* our Win386 hack needs the address of our console context */
	SetWindowWord(_this_console.hwndMain,USER_GWW_CTX,(WORD)FP_SEG(&_this_console));
	SetWindowLong(_this_console.hwndMain,USER_GWW_CTX+2,(DWORD)FP_OFF(&_this_console));
#elif TARGET_MSDOS == 16
	/* our Win16 hack needs the address of our console context */
	SetWindowLong(_this_console.hwndMain,USER_GWW_CTX,(DWORD)(&_this_console));
#endif

	/* Create the monospace font we use for terminal display */
	{
		_this_console.monoSpaceFont = CreateFont(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH | FF_DONTCARE,"Terminal");
		if (!_this_console.monoSpaceFont) {
			MessageBox(NULL,"Unable to create Font","Oops!",MB_OK);
			return 1;
		}

		{
			HWND hwnd = GetDesktopWindow();
			HDC hdc = GetDC(hwnd);
			_this_console.monoSpaceFontHeight = 12;
			if (!GetCharWidth(hdc,'A','A',&_this_console.monoSpaceFontWidth)) _this_console.monoSpaceFontWidth = 9;
			ReleaseDC(hwnd,hdc);
		}
	}

	ShowWindow(_this_console.hwndMain,nCmdShow);
	UpdateWindow(_this_console.hwndMain);
	SetWindowPos(_this_console.hwndMain,HWND_TOP,0,0,
		(_this_console.monoSpaceFontWidth * _this_console.conWidth) +
			(2 * GetSystemMetrics(SM_CXFRAME)),
		(_this_console.monoSpaceFontHeight * _this_console.conHeight) +
			(2 * GetSystemMetrics(SM_CYFRAME)) + GetSystemMetrics(SM_CYCAPTION),
		SWP_NOMOVE);

	if (setjmp(_this_console.exit_jmp) == 0)
		_main_f(0,NULL,NULL); /* <- FIXME: We need to take the command line and generate argv[]. Also generate envp[] */

	if (!_this_console.userReqClose) {
		_win_printf("\n<program terminated>");
		_this_console.allowClose = 1;
		while (GetMessage(&msg,NULL,0,0)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	else {
		if (IsWindow(_this_console.hwndMain)) {
			DestroyWindow(_this_console.hwndMain);
			while (GetMessage(&msg,NULL,0,0)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}

	DeleteObject(_this_console.monoSpaceFont);
	_this_console.monoSpaceFont = NULL;
	return 0;
}
#endif

