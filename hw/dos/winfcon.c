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
# include <shellapi.h>
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

unsigned int (*_win_dropFilesHandler)(HDROP hDrop) = NULL;

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
	struct _win_console_ctx *_ctx_console;
    _ctx_console = (void *)GetWindowLong(hwnd,USER_GWW_CTX);
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
    else if (message == WM_DROPFILES) {
        if (_win_dropFilesHandler != NULL)
            return _win_dropFilesHandler((HDROP)wparam);
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

int _win_read(int fd,void *buf,size_t sz) {
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

void _win_backspace() {
    if (_this_console.conX > 0) {
        _this_console.conX--;
		_win_update_cursor();
    }
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

#undef fflush
int _win_fflush(FILE *stream) {
    if (stream == stdout)
		_win_redraw_line_row();

    return fflush(stream);
}

/* write to the console. does NOT redraw the screen unless we get a newline or we need to scroll up */
void _win_putc(char c) {
    if (c == 8) {
        _win_backspace();
    }
    else if (c == 10) {
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

size_t _win_common_vaprintf(const char *fmt,va_list va) {
	size_t r = vsnprintf(temprintf,sizeof(temprintf)-1,fmt,va);
	int fX = _this_console.conX;
	char *t;

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
	return r;
}

size_t _win_printf(const char *fmt,...) {
	va_list va;
	size_t r;

	va_start(va,fmt);
	r = _win_common_vaprintf(fmt,va);
	va_end(va);

	return r;
}

size_t _win_fprintf(FILE *fp,const char *fmt,...) {
	va_list va;
	size_t r;

	va_start(va,fmt);

	if ((stderr && fp == stderr) || (stdout && fp == stdout))
		r = _win_common_vaprintf(fmt,va);
	else
		r = vfprintf(fp,fmt,va);

	va_end(va);
	return r;
}

void _gdi_pause() {
}

static char *cmdline_copy = NULL;
static char **cmdline_argv = NULL;
static unsigned int cmdline_argc_alloc = 0;
static unsigned int cmdline_argc = 0;

static void lpstr_to_cmdline(LPSTR lpCmdLine) {
    char *s,*start;

    /* assume: cmdline_copy == NULL, cmdline_argv == NULL, cmdline_argc_alloc == 0, cmdline_argc == 0 */
    /* do not call this twice */

    /* first, we need to duplicate the command line so we can chop it up and feed it to main() as argv[] */
    cmdline_copy = strdup(lpCmdLine);
    if (cmdline_copy == NULL) return;

    /* chop it up into argv[] */
    cmdline_argc_alloc = 16;
    cmdline_argv = malloc(sizeof(char*) * cmdline_argc_alloc);
    if (cmdline_argv == NULL) return;

    /* we don't have code to determine argv[0] yet (FIXME). assume argc == 0 */
    cmdline_argv[cmdline_argc++] = "";

    /* convert the rest of the string to argv[1]....argv[N] */
    s = cmdline_copy;
    while (*s != 0) {
        if (*s == ' ') {
            s++;
            continue;
        }

        if (*s == '\"') {
            start = ++s; /* argv[] starts just after quote */
            while (*s && *s != '\"') s++;

            if (*s) *s++ = 0; /* ASCIIZ snip (erase trailing quote) */
        }
        else {
            start = s;
            while (*s && *s != ' ') s++;

            if (*s) *s++ = 0; /* ASCIIZ snip */
        }

        /* so, "start" points at the first char of argv[]. */
        /* assume: start != NULL. we allow "" as argv[].
         * expand argv[] as needed. */
        if ((cmdline_argc+1) > cmdline_argc_alloc) {
            size_t nl = cmdline_argc_alloc + 48;
            char **np = (char**)realloc((void*)cmdline_argv,sizeof(char*) * nl);
            if (np == NULL) break;

            cmdline_argv = np;
            cmdline_argc_alloc = nl;
        }
        cmdline_argv[cmdline_argc++] = start;

        /* skip whitespace */
        while (*s == ' ') s++;
    }

    /* final argv[] */
    assert(cmdline_argc < cmdline_argc_alloc);
    cmdline_argv[cmdline_argc] = NULL;
}

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL _win_main_con_entry(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow,int (_cdecl *_main_f)(int argc,char**,char**)) {
	WNDCLASS wnd;
	MSG msg;

#if TARGET_MSDOS == 16
    /* This code no longer supports Windows 3.0 real mode.
     * Too painful to work with.
     * Remind the developer to set the "protected mode only" flag on their executable.
     * We ask code that needs to run in real mode to take the extra effort to talk to
     * the Windows API with as little abstraction as possible and it's own memory management,
     * in order to run in the tight memory constraints. */
    if (!(GetWinFlags() & WF_PMODE)) {
        MessageBox(NULL,"This application requires protected mode Windows.","Not supported",MB_OK);
        return 1;
    }
#endif

    lpstr_to_cmdline(lpCmdLine);

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

#if (TARGET_MSDOS == 32 && defined(WIN386)) || TARGET_MSDOS == 16
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
		_main_f(cmdline_argc, cmdline_argv, NULL); /* <- FIXME: We need to take the command line and generate argv[]. Also generate envp[] */

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

