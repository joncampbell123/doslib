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
#include <stdio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <hw/dos/dos.h>
#include <windows/apihelp.h>

/* display "modes", i.e. what we are displaying */
enum {
	DIM_LINEAR=0,			/* "linear" memory view (as seen from a flat 4GB paged viewpoint) */
	DIM_PHYSICAL,			/* "physical" memory view (making us a Windows equivalent of misc/hexmem1) */
	DIM_SEGMENT,			/* we're viewing memory associated with a segment */
	DIM_GLOBALHANDLE,		/* we locked a global memory object and we're showing it's contents */
};

HWND		hwndMain;
HFONT		monospaceFont = NULL;
int		monospaceFontWidth=0,monospaceFontHeight=0;
const char*	WndProcClass = "HEXMEMWINDOWS";
HINSTANCE	myInstance;
HICON		AppIcon;
RECT		hwndMainClientArea;
int		hwndMainClientRows,hwndMainClientColumns;
int		hwndMainClientAddrColumns;
int		hwndMainClientDataColumns;
HMENU		hwndMainMenu = NULL;
HMENU		hwndMainFileMenu = NULL;
HMENU		hwndMainViewMenu = NULL;
int		displayMode = DIM_LINEAR;
DWORD		displayOffset = 0;
DWORD		displayMax = 0;
char		captureTmp[4096];
char		drawTmp[512];

#if TARGET_MSDOS == 16
static volatile unsigned char faulted = 0;

/* SS:SP + 12h = SS (fault)
 * SS:SS + 10h = SP (fault)
 * SS:SP + 0Eh = FLAGS (fault)
 * SS:SP + 0Ch = CS (fault)
 * SS:SP + 0Ah = IP (fault)
 * SS:SP + 08h = handle (internal)
 * SS:SP + 06h = interrupt number
 * SS:SP + 04h = AX (to load into DS)
 * SS:SP + 02h = CS (toolhelp.dll)
 * SS:SP + 00h = IP (toolhelp.dll)
 *
 * to pass exception on: RETF
 * to restart instruction: clear first 10 bytes of the stack, and IRET (??) */
static void __declspec(naked) fault_pf_toolhelp() {
	__asm {
		.386p
		push		ds
		push		ax
		push		bp
		mov		bp,sp

		/* is this a page fault? */
		cmp		word ptr [bp+6+6],14	/* SS:SP + 06h = interrupt number */
		jnz		pass_on

		/* set the faulted flag, change the return address, then IRET directly back */
		mov		ax,seg faulted
		mov		ds,ax
		mov		faulted,1
		add		word ptr [bp+6+10],3
		pop		bp
		pop		ax
		pop		ds
		add		sp,0Ah			/* throw away handle, int number, and CS:IP return */
		iret

pass_on:	retf
	}
}

static void __declspec(naked) fault_pf_dpmi() { /* DPMI exception handler */
	__asm {
		.386p
		push		ds
		push		ax
		push		bp
		mov		bp,sp
		mov		ax,seg faulted
		mov		ds,ax
		mov		faulted,1
		add		word ptr [bp+6+6],3
		pop		bp
		pop		ax
		pop		ds
		retf
	}
}
#endif

/* Notes and findings so far:

   Windows 3.1, 16-bit: The first 1MB of RAM + 64KB is mapped 1:1 to what you would get in DOS.
                        At 0x80001000 begins the kernel-mode VXD/386 driver list.
			The remainder is various global memory pools.

   Windows 3.1, 32-bit (Win32s): Same as 16-bit view, offset by 64KB. The first 64KB is mapped out.

   Windows 95, 16-bit: Same as Windows 3.1, though with Windows 95 components resident.
   Windows 95, 32-bit: Same as 16-bit world, though the first 64KB is read-only.
   Windows 98, 16-bit: Same as Windows 95
   Windows 98, 32-bit: 32-bit side of the world has the first 64KB mapped out entirely.
   Windows ME, 16-bit: Same as Windows 95
   Windows ME, 32-bit: Same as Windows 98, first 64KB mapped out, except for strange behavior
                       around the 0xF000-0xFFFF area: that region suddenly becomes readable from
                       0xFE61-0xFFFF which means that someone in the system traps reads and actively
                       compares the fault location to pass or fail it. Weird. Full testing is impossible
		       because KERNEL32.DLL will automatically terminate our program if we do 160
		       consecutive faults in a row, ignoring our exception handler. */

static int CaptureMemoryLinear(DWORD memofs,unsigned int howmuch) {
#if TARGET_MSDOS == 32
	volatile unsigned char *s = (volatile unsigned char*)memofs;
	volatile unsigned char *d = captureTmp;
	volatile unsigned char *t,*sf,cc;
	int fail = 0;

	/* copy one page at a time */
	sf = s + howmuch;
	if (s > sf) {
		/* alternate version for top of 4GB access */
		sf = (volatile unsigned char*)(~0UL);
		do {
			__try {
				do {
					cc = *s;
					*d++ = cc;
				} while ((s++) != sf);
			}
			__except(1) {
				do {
					*d++ = 0xFF;
					fail++;
				} while ((s++) != sf);
			}
		} while (s != sf && s != (volatile unsigned char*)0);
	}
	else {
		while (s < sf) {
			t = (volatile unsigned char*)((((DWORD)s) | 0xFFF) + 1);

			/* handle 4GB wraparound. but if sf had wrapped around, we would be in the upper portion of this function */
			if (t == (volatile unsigned char*)0) t = sf;
			else if (t > sf) t = sf;

			__try {
				while (s < t) {
					cc = *s;
					*d++ = cc;
					s++;
				}
			}
			__except(1) {
				while (s < t) {
					*d++ = 0xFF;
					fail++;
					s++;
				}
			}
		}
	}

	return fail;
#else
	if (windows_mode == WINDOWS_REAL) {
		unsigned char *d = captureTmp,cc=0;
		unsigned char far *sseg;
		unsigned int s=0,sf;

		sseg = MK_FP((unsigned int)(memofs >> 4UL),0);
		s = (unsigned int)(memofs & 0xFUL);
		sf = s + howmuch;
		while (s < sf) {
			cc = sseg[s];
			s++; *d++ = cc;
		}

		return 0;
	}
	else {
		volatile unsigned char *d = captureTmp,cc=0;
		UINT selector,my_ds=0,my_ss=0,my_cs=0;
		unsigned int s=0,sf,t;
		void far *oldDPMI;
		int tlhelp = 0;
		int fail=0;

		/* Windows NT warning: This code works fine, but NTVDM.EXE will not pass Page Fault exceptions
		   down to us. Instead page faults will simply cause NTVDM.EXE to exit. So beware: if you're
		   scrolling through memory and suddenly the program disappears---that's why.

		   We hook Page Fault anyway in the vain hope that some version of NT lets it pass through. */

		/* OS/2 warning: Win-on-OS/2 also doesn't provide a way to catch page faults. */

		/* Windows 3.0: Our DPMI-based hack also fails to catch page faults in 386 Enhanced Mode. Use Standard Mode if you need to browse through RAM */

		/* WARNING WARNING WARNING!!!!!

		   On actual hardware and most emulators, this code succeeds at catching page faults and safely
		   reflecting back to the main loop.

		   Tested and compatible:
		   Windows 3.1 under QEMU
		   Windows 95 under VirtualBox
		   Windows 98 under VirtualBox

		   DOSBox 0.74 however does NOT properly emulate the Page Fault exception (at least in a way
		   that Windows 3.1 expects to handle). If you run this program under Windows 3.1/3.0 under
		   DOSBox 0.74 and this code causes a Page Fault, the next thing you will see is either a)
		   Windows freeze solid or b) Windows crash-dump to the DOS prompt or c) Windows *TRY* to
		   crash dump to the DOS prompt and hang.

		   Note that 32-bit builds of this program running under Windows 3.1 + Win32s are able to
		   page fault without crashing erratically under DOSBox, which means that the problem probably
		   lies in DOSBox not properly emulating the 16-bit protected mode version of the Page Fault
		   exception.

		   I will be patching DOSBox-X to better emulate the Page Fault and resolve this issue. */

		/* We really only need to trap Page Fault from 386 enhanced mode Windows 3.x or Windows 9x/ME/NT.
		   Windows 3.x "standard mode" does not involve paging */
		if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) {
			/* Windows 3.1 or higher: Use Toolhelp.
			   Windows 3.0: Use DPMI hacks */
			tlhelp = (windows_mode == WINDOWS_NT) || (windows_version >= ((3 << 8) + 10)); /* Windows 3.1 or higher */

			/* we may cause page faults. be prepared */
			if (tlhelp) {
				if (!ToolHelpInit() || __InterruptRegister == NULL || __InterruptUnRegister == NULL)
					return howmuch;

				if (!__InterruptRegister(NULL,MakeProcInstance((FARPROC)fault_pf_toolhelp,myInstance)))
					return howmuch;
			}
			else {
				oldDPMI = win16_getexhandler(14);
				if (!win16_setexhandler(14,fault_pf_dpmi)) return howmuch;
			}
		}

		__asm {
			mov my_cs,cs
			mov my_ds,ds
			mov my_ss,ss
		}

		selector = AllocSelector(my_ds);
		if (selector == 0) {
			if (tlhelp) __InterruptUnRegister(NULL);
			else win16_setexhandler(6,oldDPMI);
			return howmuch;
		}
		SetSelectorBase(selector,memofs & 0xFFFFF000UL);
		SetSelectorLimit(selector,howmuch + 0x1000UL);

		/* we need to lock our code, data, and stack in place */
		LockSegment(my_cs);
		LockSegment(my_ds);
		LockSegment(my_ss);

		s = (unsigned int)(memofs & 0xFFFUL);
		sf = s + howmuch;
		while (s < sf) {
			t = (s | 0xFFFU) + 1U;
			if (t > sf) t = sf;
			faulted = 0;

			while (s < t) {
				/* we have to do it this way to exert better control over how we resume from fault */
				__asm {
					push	si
					push	ds
					mov	si,s
					mov	ax,selector
					mov	ds,ax

					nop
					nop
					mov	al,[si]
					nop
					nop

					pop	ds
					pop	si
					mov	cc,al
				}
				if (faulted) break;
				s++; *d++ = cc;
			}

			while (s < t) {
				s++; *d++ = 0xFF; fail++;
			}
		}

		UnlockSegment(my_cs);
		UnlockSegment(my_ds);
		UnlockSegment(my_ss);

		if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) {
			/* remove page fault handler */
			if (tlhelp) __InterruptUnRegister(NULL);
			else win16_setexhandler(6,oldDPMI);
		}

		FreeSelector(selector);
		return fail;
	}
#endif
}

static int CaptureMemory(DWORD memofs,unsigned int howmuch) {
	int fail;

	if (howmuch > sizeof(captureTmp) || howmuch == 0) return howmuch;

	/* the actual routines don't handle 4GB wrap very well */
	if ((memofs+((DWORD)howmuch)) < memofs)
		howmuch = (unsigned int)(0UL - memofs);

	switch (displayMode) {
		case DIM_LINEAR:
			fail = CaptureMemoryLinear(memofs,howmuch);
			break;
		default:
			memset(captureTmp,0xFF,howmuch);
			fail += howmuch;
			break;
	}

	return fail;
}

static const char *hexes = "0123456789ABCDEF";

static void RedrawMemory(HWND hwnd,HDC hdc) {
	unsigned char *src,cc;
	int rowspercopy;
	int x,y,i,fail;
	DWORD memofs;
	HFONT of;
	int rem;

	if (hwndMainClientDataColumns == 0)
		return;

	SetBkMode(hdc,OPAQUE);
	SetBkColor(hdc,RGB(255,255,255));
	of = (HFONT)SelectObject(hdc,monospaceFont);
	rowspercopy = sizeof(captureTmp) / hwndMainClientDataColumns;

	memofs = displayOffset;
	for (y=0;y < hwndMainClientRows;y++) {
		sprintf(drawTmp,"%08lX ",(unsigned long)memofs);
		memofs += hwndMainClientDataColumns;
		TextOut(hdc,0,y*monospaceFontHeight,drawTmp,hwndMainClientAddrColumns);
	}

	memofs = displayOffset;
	for (y=0;y < hwndMainClientRows;) {
		src = captureTmp;
		rem = hwndMainClientRows - y; if (rem > rowspercopy) rem = rowspercopy;
		fail = CaptureMemory(memofs,rem * hwndMainClientDataColumns);

		if (fail > 0)
			SetTextColor(hdc,RGB(128,0,0));
		else
			SetTextColor(hdc,RGB(0,0,0));

		for (i=0;i < rem;i++) {
			for (x=0;x < hwndMainClientDataColumns;x++) {
				cc = *src++;
				drawTmp[(x*3)+0] = hexes[cc>>4];
				drawTmp[(x*3)+1] = hexes[cc&0xF];
				drawTmp[(x*3)+2] = ' ';

				if (cc == 0) cc = '.';
				drawTmp[(hwndMainClientDataColumns*3)+x] = cc;
			}
			drawTmp[hwndMainClientDataColumns*4] = 0;
			TextOut(hdc,(hwndMainClientAddrColumns+1)*monospaceFontWidth,(y+i)*monospaceFontHeight,drawTmp,hwndMainClientDataColumns*4);
		}
		y += rowspercopy;
	}

	SelectObject(hdc,of);
}

static void ClearDisplayMode() {
}

static void SetDisplayModeLinear(DWORD ofs) {
	ClearDisplayMode();

	displayMode = DIM_LINEAR;
	displayOffset = ofs;
	displayMax = 0xFFFFFFFFUL;
}

static void UpdateClientAreaValues() {
	GetClientRect(hwndMain,&hwndMainClientArea);
	hwndMainClientRows = (hwndMainClientArea.bottom + monospaceFontHeight - 1) / monospaceFontHeight;
	hwndMainClientColumns = hwndMainClientArea.right / monospaceFontWidth;
	hwndMainClientAddrColumns = 8;
	hwndMainClientDataColumns = (hwndMainClientColumns - (hwndMainClientAddrColumns + 1)) / 4;
	if (hwndMainClientDataColumns == 15 || hwndMainClientDataColumns == 17) hwndMainClientDataColumns = 16;
	if (hwndMainClientDataColumns < 0) hwndMainClientDataColumns = 0;
	if (hwndMainClientDataColumns > 128) hwndMainClientDataColumns = 128;
}

BOOL CALLBACK FAR __loadds GoToDlgProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_INITDIALOG) {
		char tmp[128];
		sprintf(tmp,"0x%08lX",(unsigned long)displayOffset);
		SetDlgItemText(hwnd,IDC_GOTO_VALUE,tmp);
		return TRUE;
	}
	else if (message == WM_COMMAND) {
		if (wparam == IDOK) {
			char tmp[128];
			GetDlgItemText(hwnd,IDC_GOTO_VALUE,tmp,sizeof(tmp));
			displayOffset = strtoul(tmp,NULL,0);
			EndDialog(hwnd,1);
		}
		else if (wparam == IDCANCEL) {
			EndDialog(hwnd,0);
		}
	}

	return FALSE;
}

#if TARGET_MSDOS == 16
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
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor(NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
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
	else if (message == WM_KEYDOWN) {
		if (wparam == VK_LEFT) {
			displayOffset--;
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_RIGHT) {
			displayOffset++;
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_UP) {
			displayOffset -= hwndMainClientDataColumns;
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_DOWN) {
			displayOffset += hwndMainClientDataColumns;
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_PRIOR) {/*page up*/
			displayOffset -= hwndMainClientDataColumns * (hwndMainClientRows-2);
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
		else if (wparam == VK_NEXT) {/*page down*/
			displayOffset += hwndMainClientDataColumns * (hwndMainClientRows-2);
			if (GetAsyncKeyState(VK_SHIFT)) displayOffset &= ~(0xFUL);
			InvalidateRect(hwnd,NULL,FALSE);
		}
	}
	else if (message == WM_COMMAND) {
		if (HIWORD(lparam) == 0) { /* from a menu */
			switch (LOWORD(wparam)) {
				case IDC_FILE_QUIT:
					SendMessage(hwnd,WM_CLOSE,0,0);
					break;
				case IDC_FILE_GO_TO:
					if (DialogBox(myInstance,MAKEINTRESOURCE(IDD_GOTO),hwnd,MakeProcInstance(GoToDlgProc,myInstance)))
						InvalidateRect(hwnd,NULL,FALSE);
					break;
			}
		}
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			PAINTSTRUCT ps;

			UpdateClientAreaValues();
			BeginPaint(hwnd,&ps);
			RedrawMemory(hwnd,ps.hdc);
			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

static int CreateAppMenu() {
	hwndMainMenu = CreateMenu();
	if (hwndMainMenu == NULL) return 0;

	hwndMainFileMenu = CreateMenu();
	if (hwndMainFileMenu == NULL) return 0;

	if (!AppendMenu(hwndMainFileMenu,MF_STRING|MF_ENABLED,IDC_FILE_GO_TO,"&Go to..."))
		return 0;

	if (!AppendMenu(hwndMainFileMenu,MF_SEPARATOR,0,""))
		return 0;

	if (!AppendMenu(hwndMainFileMenu,MF_STRING|MF_ENABLED,IDC_FILE_QUIT,"&Quit"))
		return 0;

	if (!AppendMenu(hwndMainMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)hwndMainFileMenu,"&File"))
		return 0;

	hwndMainViewMenu = CreateMenu();
	if (hwndMainViewMenu == NULL) return 0;

	if (!AppendMenu(hwndMainViewMenu,MF_STRING|MF_ENABLED|MF_CHECKED,IDC_VIEW_LINEAR,"&Linear memory map"))
		return 0;

	if (!AppendMenu(hwndMainMenu,MF_POPUP|MF_STRING|MF_ENABLED,(UINT)hwndMainViewMenu,"&View"))
		return 0;

	return 1;
}

/* NOTES:
 *   For Win16, programmers generally use WINAPI WinMain(...) but WINAPI is defined in such a way
 *   that it always makes the function prolog return FAR. Unfortunately, when Watcom C's runtime
 *   calls this function in a memory model that's compact or small, the call is made as if NEAR,
 *   not FAR. To avoid a GPF or crash on return, we must simply declare it PASCAL instead. */
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	/* some important decision making rests on what version of Windows we're running under */
	probe_dos();
	detect_windows();

#if TARGET_MSDOS == 16
	if (windows_mode == WINDOWS_NT) {
		if (MessageBox(NULL,
			"The 16-bit version of this program does not have the ability to catch Page Faults under Windows NT.\n"
			"If you browse a region of memory that causes a Page Fault, you will see this program disappear without warning.\n"
			"Proceed?",
			"WARNING",MB_YESNO) == IDNO)
			return 1;
	}
	else if (windows_mode == WINDOWS_OS2) {
		if (MessageBox(NULL,
			"The 16-bit version of this program does not have the ability to catch Page Faults under OS/2.\n"
			"If you browse a region of memory that causes a Page Fault, you will see the appropriate notifcation from OS/2.\n"
			"Proceed?",
			"WARNING",MB_YESNO) == IDNO)
			return 1;
	}
#endif

	myInstance = hInstance;

	/* FIXME: Windows 3.0 Real Mode: Why can't we load our own app icon? */
	AppIcon = LoadIcon(hInstance,MAKEINTRESOURCE(IDI_APPICON));
	if (!AppIcon) MessageBox(NULL,"Unable to load app icon","Oops!",MB_OK);

#if TARGET_MSDOS == 16
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
#if TARGET_MSDOS == 16
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

	monospaceFont = CreateFont(-12,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FIXED_PITCH | FF_DONTCARE,"Terminal");
	if (!monospaceFont) {
		MessageBox(NULL,"Unable to create Font","Oops!",MB_OK);
		return 1;
	}

	{
		HWND hwnd = GetDesktopWindow();
		HDC hdc = GetDC(hwnd);
		monospaceFontHeight = 12;
		if (!GetCharWidth(hdc,'A','A',&monospaceFontWidth)) monospaceFontWidth = 9;
		ReleaseDC(hwnd,hdc);
	}

	if (!CreateAppMenu())
		return 1;

	/* default to "linear" view */
	SetDisplayModeLinear(0xC0000);

	hwndMain = CreateWindow(WndProcClass,"HEXMEM memory dumping tool",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		(monospaceFontWidth*80)+(GetSystemMetrics(SM_CXFRAME)*2),
		(monospaceFontHeight*25)+(GetSystemMetrics(SM_CYFRAME)*2)+GetSystemMetrics(SM_CYCAPTION)-GetSystemMetrics(SM_CYBORDER)+GetSystemMetrics(SM_CYMENU),
		NULL,NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox(NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	SetMenu(hwndMain,hwndMainMenu);
	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	while (GetMessage(&msg,NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	DestroyMenu(hwndMainMenu);
	hwndMainMenu = NULL;
	DeleteObject(monospaceFont);
	monospaceFont = NULL;
	return msg.wParam;
}

