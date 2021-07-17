
#if defined(USE_WIN32)
#include <windows.h>
#include <mmsystem.h>

#include <conio.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dosboxid/iglib.h> /* for debugging */

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

BITMAPINFO*				hwndMainDIB = NULL;
HPALETTE				hwndMainPAL = NULL;
HPALETTE				hwndMainPALPrev = NULL;
PALETTEENTRY				win_pal[256];
unsigned char*				win_dib = NULL;
unsigned char*				win_dib_first_row = NULL;
int					win_dib_pitch = 0;
DWORD					win32_tick_base = 0;
ifevidinfo_t				ifevidinfo_win32;
bool					winQuit = false;
bool					winIsDestroying = false;
bool					winScreenIsPal = false;
bool					win95 = false;
const char*				hwndMainClassName = "IFICTIONWIN32";
HINSTANCE				myInstance = NULL;
HWND					hwndMain = NULL;
uint32_t				win32_mod_flags = 0;
IFEMouseStatus				ifemousestat;
bool					mousecap_on = false;
HRGN					upd_region = NULL;
HRGN					upd_rect = NULL;
bool					upd_region_valid = false;

void MakeRgnEmpty(HRGN r) {
	SetRectRgn(r,0,0,0,0); /* region is x1 <= x < x2, y1 <= y < y2, therefore this rectangle makes an empty region */
}

void priv_CaptureMouse(const bool cap) {
	if (cap && !mousecap_on) {
		SetCapture(hwndMain);
		mousecap_on = true;
	}
	else if (!cap && mousecap_on) {
		ReleaseCapture();
		mousecap_on = false;
	}
}

void priv_ProcessMouseMotion(const WPARAM wParam,int x,int y) {
	IFEMouseEvent me;

	ifemousestat.x = x;
	ifemousestat.y = y;

	memset(&me,0,sizeof(me));
	me.pstatus = ifemousestat.status;

	/* wParam for mouse messages indicate which buttons are down */
	if (wParam & MK_LBUTTON)
		ifemousestat.status |= IFEMouseStatus_LBUTTON;
	else
		ifemousestat.status &= ~IFEMouseStatus_LBUTTON;

	if (wParam & MK_MBUTTON)
		ifemousestat.status |= IFEMouseStatus_MBUTTON;
	else
		ifemousestat.status &= ~IFEMouseStatus_MBUTTON;

	if (wParam & MK_RBUTTON)
		ifemousestat.status |= IFEMouseStatus_RBUTTON;
	else
		ifemousestat.status &= ~IFEMouseStatus_RBUTTON;

	if (ifemousestat.status & (IFEMouseStatus_LBUTTON|IFEMouseStatus_MBUTTON|IFEMouseStatus_RBUTTON))
		priv_CaptureMouse(true);
	else
		priv_CaptureMouse(false);

	me.x = ifemousestat.x;
	me.y = ifemousestat.y;
	me.status = ifemousestat.status;

	IFEDBG("Mouse event x=%d y=%d pstatus=%08x status=%08x",
		ifemousestat.x,
		ifemousestat.y,
		(unsigned int)me.pstatus,
		(unsigned int)me.status);

	/* Add only button events */
	if ((me.status^me.pstatus) & (IFEMouseStatus_LBUTTON|IFEMouseStatus_MBUTTON|IFEMouseStatus_RBUTTON)) {
		if (!IFEMouseQueue.add(me))
			IFEDBG("Mouse event overrun");
	}
}

static void p_SetPaletteColors(const unsigned int first,const unsigned int count,IFEPaletteEntry *pal) {
	unsigned int i;

	priv_SetPaletteColorsRangeCheck(first,count);

	if (winScreenIsPal) {
		for (i=0;i < count;i++) {
			win_pal[i].peRed = pal[i].r;
			win_pal[i].peGreen = pal[i].g;
			win_pal[i].peBlue = pal[i].b;
			win_pal[i].peFlags = 0;
		}

		SetPaletteEntries(hwndMainPAL,first,count,win_pal);

		{
			HPALETTE oldPal;
			HDC hDC;

			hDC = GetDC(hwndMain);
			oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
			RealizePalette(hDC);
			SelectPalette(hDC,oldPal,FALSE);
			ReleaseDC(hwndMain,hDC);
			InvalidateRect(hwndMain,NULL,FALSE);
		}
	}
	else {
		for (i=0;i < count;i++) {
			hwndMainDIB->bmiColors[i+first].rgbRed = pal[i].r;
			hwndMainDIB->bmiColors[i+first].rgbGreen = pal[i].g;
			hwndMainDIB->bmiColors[i+first].rgbBlue = pal[i].b;
		}
	}
}

static uint32_t p_GetTicks(void) {
	return uint32_t(timeGetTime() - win32_tick_base);
}

static void p_ResetTicks(const uint32_t base) {
	win32_tick_base += base;
}

static void p_UpdateFullScreen(void) {
	HDC hDC = GetDC(hwndMain);
	HPALETTE oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);

	SetDIBitsToDevice(hDC,
		/*dest x/y*/0,0,
		abs((int)hwndMainDIB->bmiHeader.biWidth),
		abs((int)hwndMainDIB->bmiHeader.biHeight),
		/*src x/y*/0,0,
		/*starting scan/clines*/0,abs((int)hwndMainDIB->bmiHeader.biHeight),
		win_dib,
		hwndMainDIB,
		winScreenIsPal ? DIB_PAL_COLORS : DIB_RGB_COLORS);

	SelectPalette(hDC,oldPal,FALSE);
	ReleaseDC(hwndMain,hDC);

	MakeRgnEmpty(upd_region);
	upd_region_valid = false;
}

static ifevidinfo_t* p_GetVidInfo(void) {
	return &ifevidinfo_win32;
}

static bool p_UserWantsToQuit(void) {
	return winQuit;
}

static void p_CheckEvents(void) {
	MSG msg;

	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static void p_WaitEvent(const int wait_ms) {
	MSG msg;

	(void)wait_ms;

	if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static bool p_BeginScreenDraw(void) {
	return true; // nothing to do
}

static void p_EndScreenDraw(void) {
	// nothing to do
}

static void p_ShutdownVideo(void) {
	priv_CaptureMouse(false);
	if (hwndMainPAL != NULL) {
		DeleteObject((HGDIOBJ)hwndMainPAL);
		hwndMainPALPrev = NULL;
		hwndMainPAL = NULL;
	}
	if (hwndMainDIB != NULL) {
		free((void*)hwndMainDIB);
		hwndMainDIB = NULL;
	}
	if (hwndMain != NULL) {
		winIsDestroying = true;
		DestroyWindow(hwndMain);
		winIsDestroying = false;
		hwndMain = NULL;
	}
	if (win_dib != NULL) {
		ifevidinfo_win32.buf_base = ifevidinfo_win32.buf_first_row = NULL;
		free((void*)win_dib);
		win_dib = NULL;
	}
	if (upd_region != NULL) {
		DeleteObject((HGDIOBJ)upd_region);
		upd_region = NULL;
	}
	if (upd_rect != NULL) {
		DeleteObject((HGDIOBJ)upd_rect);
		upd_rect = NULL;
	}
}

static void p_InitVideo(void) {
	if (hwndMain == NULL) {
		int sw,sh;

		mousecap_on = false;
		memset(&ifemousestat,0,sizeof(ifemousestat));
		memset(&ifevidinfo_win32,0,sizeof(ifevidinfo_win32));

		ifevidinfo_win32.width = 640;
		ifevidinfo_win32.height = 480;

		sw = GetSystemMetrics(SM_CXSCREEN);
		sh = GetSystemMetrics(SM_CYSCREEN);

		{
			HDC hDC = GetDC(NULL); /* the screen */
			if ((GetDeviceCaps(hDC,RASTERCAPS) & RC_PALETTE) && (GetDeviceCaps(hDC,BITSPIXEL) == 8))
				winScreenIsPal = true;
			else
				winScreenIsPal = false;
			ReleaseDC(NULL,hDC);
		}

		{
			DWORD dwStyle = WS_OVERLAPPED|WS_SYSMENU|WS_CAPTION|WS_BORDER;
			RECT um;

			um.top = 0;
			um.left = 0;
			um.right = 640;
			um.bottom = 480;
			if (sw == 640 && sh == 480) {
				/* make it borderless, which is impossible if Windows sees a WS_OVERLAPPED style combo */
				dwStyle = WS_POPUP;
			}
			else {
				AdjustWindowRect(&um,dwStyle,FALSE);
			}

			hwndMain = CreateWindow(hwndMainClassName,"",dwStyle,
				CW_USEDEFAULT,CW_USEDEFAULT,um.right - um.left,um.bottom - um.top,
				NULL,NULL,
				myInstance,
				NULL);
		}

		if (hwndMain == NULL)
			IFEFatalError("CreateWindow failed");

		{
			int ww,wh;

			{
				RECT um = {0,0,0,0};
				GetWindowRect(hwndMain,&um);
				ww = (um.right - um.left);
				wh = (um.bottom - um.top);
			}
			SetWindowPos(hwndMain,NULL,(sw - ww) / 2,(sh - wh) / 2,0,0,SWP_NOSIZE|SWP_SHOWWINDOW);
		}

		hwndMainDIB = (BITMAPINFO*)calloc(1,sizeof(BITMAPINFOHEADER) + (256 * 2/*16-bit integers, DIB_PAL_COLORS*/) + 4096/*for good measure*/);
		if (hwndMainDIB == NULL)
			IFEFatalError("hwndMainDIB malloc fail");

		hwndMainDIB->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		hwndMainDIB->bmiHeader.biWidth = 640;
		if (win95)
			hwndMainDIB->bmiHeader.biHeight = -480; /* top down, Windows 95 */
		else
			hwndMainDIB->bmiHeader.biHeight = 480; /* bottom up, Windows 3.1 */
		hwndMainDIB->bmiHeader.biPlanes = 1;
		hwndMainDIB->bmiHeader.biBitCount = 8;
		hwndMainDIB->bmiHeader.biCompression = 0;
		hwndMainDIB->bmiHeader.biSizeImage = 640*480; /* NTS: pitch is width rounded to a multiple of 4 */
		hwndMainDIB->bmiHeader.biClrUsed = 256;
		hwndMainDIB->bmiHeader.biClrImportant = 256;
		if (winScreenIsPal) { /* map 1:1 to logical palette */
			uint16_t *p = (uint16_t*)( &(hwndMainDIB->bmiColors[0]) );
			LOGPALETTE* pal;
			unsigned int i;

			for (i=0;i < 256;i++) p[i] = (uint16_t)i;

			pal = (LOGPALETTE*)calloc(1,sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * 256) + 4096);
			if (pal == NULL) IFEFatalError("LOGPALETTE malloc fail");

			pal->palVersion = 0x300;
			pal->palNumEntries = 256;
			for (i=0;i < 256;i++) {
				pal->palPalEntry[i].peRed = (unsigned char)i;
				pal->palPalEntry[i].peGreen = (unsigned char)i;
				pal->palPalEntry[i].peBlue = (unsigned char)i;
				pal->palPalEntry[i].peFlags = 0;
			}

			hwndMainPAL = CreatePalette(pal);
			if (hwndMainPAL == NULL)
				IFEFatalError("CreatePalette fail");

			free((void*)pal);
		}
		else {
			hwndMainPALPrev = NULL;
			hwndMainPAL = NULL;
		}

		win_dib = (unsigned char*)malloc(hwndMainDIB->bmiHeader.biSizeImage);
		if (win_dib == NULL)
			IFEFatalError("win_dib malloc fail");

		/* NTS: Windows 3.1 with Win32s does not provide any function to draw top-down DIBs.
		 *      Windows 95 however does support top down DIBs, so this code should consider auto-detecting that.
		 *      To simplify other code, pitch and first row are computed here. */
		if ((int)(hwndMainDIB->bmiHeader.biHeight) < 0) {
			win_dib_first_row = win_dib;
			win_dib_pitch = (int)hwndMainDIB->bmiHeader.biWidth;
		}
		else {
			win_dib_first_row = win_dib + (hwndMainDIB->bmiHeader.biSizeImage - hwndMainDIB->bmiHeader.biWidth); /* FIXME: What about future code for != 8bpp? Also alignment. */
			win_dib_pitch = -((int)hwndMainDIB->bmiHeader.biWidth);
		}

		ifevidinfo_win32.buf_base = win_dib;
		ifevidinfo_win32.buf_first_row = win_dib_first_row;
		ifevidinfo_win32.buf_pitch = win_dib_pitch;
		ifevidinfo_win32.buf_alloc = ifevidinfo_win32.buf_size = (uint32_t)(hwndMainDIB->bmiHeader.biSizeImage);

		/* HRGN objects for partial screen updates.
		 * Let Windows use it's GDI system rather than reimplementing it ourselves */
		upd_region = CreateRectRgn(0,0,0,0); /* empty RGN */
		if (upd_region == NULL)
			IFEFatalError("Update Region create fail 1");
		upd_rect = CreateRectRgn(0,0,0,0); /* empty RGN */
		if (upd_rect == NULL)
			IFEFatalError("Update Region create fail 2");
		upd_region_valid = false;

		ifeapi->UpdateFullScreen();
		ifeapi->CheckEvents();

		IFECompleteVideoInit();
	}
}

void p_FlushKeyboardInput(void) {
	IFEKeyQueueEmptyAll();
}

IFEKeyEvent *p_GetRawKeyboardInput(void) {
	return IFEKeyQueue.get();
}

IFECookedKeyEvent *p_GetCookedKeyboardInput(void) {
	return IFECookedKeyQueue.get();
}

IFEMouseStatus *p_GetMouseStatus(void) {
	return &ifemousestat;
}

void p_FlushMouseInput(void) {
	IFEMouseQueueEmptyAll();
	ifemousestat.status = 0;
	priv_CaptureMouse(false);
}

IFEMouseEvent *p_GetMouseInput(void) {
	return IFEMouseQueue.get();
}

void p_UpdateScreen(void) {
	if (upd_region_valid) {
		HDC hDC = GetDC(hwndMain);
		HPALETTE oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
		HRGN oldRgn = (HRGN)SelectObject(hDC,upd_region);

		SetDIBitsToDevice(hDC,
			/*dest x/y*/0,0,
			abs((int)hwndMainDIB->bmiHeader.biWidth),
			abs((int)hwndMainDIB->bmiHeader.biHeight),
			/*src x/y*/0,0,
			/*starting scan/clines*/0,abs((int)hwndMainDIB->bmiHeader.biHeight),
			win_dib,
			hwndMainDIB,
			winScreenIsPal ? DIB_PAL_COLORS : DIB_RGB_COLORS);

		SelectPalette(hDC,oldPal,FALSE);
		SelectObject(hDC,oldRgn);
		ReleaseDC(hwndMain,hDC);

		MakeRgnEmpty(upd_region);
		upd_region_valid = false;
	}
}

void p_AddScreenUpdate(int x1,int y1,int x2,int y2) {
	if (x1 < x2 && y1 < y2) {
		upd_region_valid = true;
		SetRectRgn(upd_rect,x1,y1,x2,y2); /* "The region does not include the lower and right boundaries of the rectangle", same as this API */
		if (CombineRgn(upd_region,upd_region,upd_rect,RGN_OR) == ERROR)
			IFEDBG("CombineRgn error");
	}
}

ifeapi_t ifeapi_win32 = {
	"Win32",
	p_SetPaletteColors,
	p_GetTicks,
	p_ResetTicks,
	p_UpdateFullScreen,
	p_GetVidInfo,
	p_UserWantsToQuit,
	p_CheckEvents,
	p_WaitEvent,
	p_BeginScreenDraw,
	p_EndScreenDraw,
	p_ShutdownVideo,
	p_InitVideo,
	p_FlushKeyboardInput,
	p_GetRawKeyboardInput,
	p_GetCookedKeyboardInput,
	p_GetMouseStatus,
	p_FlushMouseInput,
	p_GetMouseInput,
	p_UpdateScreen,
	p_AddScreenUpdate
};

void UpdateWin32ModFlags(void) {
	win32_mod_flags = 0;

	/* NTS: LSHIFT/RSHIFT works with later versions of Windows.
	 *      Windows 3.1 only understands SHIFT. Same with CTRL, ALT.
	 *      The Windows 3.1 SDK doesn't even list L/R versions of those keys. */

	if (GetKeyState(VK_LSHIFT)&0x8000u)/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_LSHIFT;
	if (GetKeyState(VK_RSHIFT)&0x8000u)/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_RSHIFT;
	if (!(win32_mod_flags & (IFEKeyEvent_FLAG_LSHIFT|IFEKeyEvent_FLAG_RSHIFT)) && (GetKeyState(VK_SHIFT)&0x8000u))/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_LSHIFT|IFEKeyEvent_FLAG_RSHIFT;

	if (GetKeyState(VK_LMENU)&0x8000u)/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_LALT;
	if (GetKeyState(VK_RMENU)&0x8000u)/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_RALT;
	if (!(win32_mod_flags & (IFEKeyEvent_FLAG_LALT|IFEKeyEvent_FLAG_RALT)) && (GetKeyState(VK_MENU)&0x8000u))/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_LALT|IFEKeyEvent_FLAG_RALT;

	if (GetKeyState(VK_LCONTROL)&0x8000u)/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_LCTRL;
	if (GetKeyState(VK_RCONTROL)&0x8000u)/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_RCTRL;
	if (!(win32_mod_flags & (IFEKeyEvent_FLAG_LCTRL|IFEKeyEvent_FLAG_RCTRL)) && (GetKeyState(VK_CONTROL)&0x8000u))/*MSB=down*/
		win32_mod_flags |= IFEKeyEvent_FLAG_LCTRL|IFEKeyEvent_FLAG_RCTRL;

	if (GetKeyState(VK_CAPITAL)&0x0001u)/*LSB=toggled on*/
		win32_mod_flags |= IFEKeyEvent_FLAG_CAPS;
	if (GetKeyState(VK_NUMLOCK)&0x0001u)/*LSB=toggled on*/
		win32_mod_flags |= IFEKeyEvent_FLAG_NUMLOCK;
}

void win32keyfill(IFEKeyEvent &ke,WPARAM w,LPARAM l) {
	(void)l;

#define MAP(x,y) \
	case x: ke.code = (uint32_t)y; break;

	switch (w) {
		MAP(VK_RETURN,             IFEKEY_RETURN);
		MAP(VK_ESCAPE,             IFEKEY_ESCAPE);
		MAP(VK_BACK,               IFEKEY_BACKSPACE);
		MAP(VK_TAB,                IFEKEY_TAB);
		MAP(VK_SPACE,              IFEKEY_SPACE);
		MAP('A',                   IFEKEY_A);
		MAP('B',                   IFEKEY_B);
		MAP('C',                   IFEKEY_C);
		MAP('D',                   IFEKEY_D);
		MAP('E',                   IFEKEY_E);
		MAP('F',                   IFEKEY_F);
		MAP('G',                   IFEKEY_G);
		MAP('H',                   IFEKEY_H);
		MAP('I',                   IFEKEY_I);
		MAP('J',                   IFEKEY_J);
		MAP('K',                   IFEKEY_K);
		MAP('L',                   IFEKEY_L);
		MAP('M',                   IFEKEY_M);
		MAP('N',                   IFEKEY_N);
		MAP('O',                   IFEKEY_O);
		MAP('P',                   IFEKEY_P);
		MAP('Q',                   IFEKEY_Q);
		MAP('R',                   IFEKEY_R);
		MAP('S',                   IFEKEY_S);
		MAP('T',                   IFEKEY_T);
		MAP('U',                   IFEKEY_U);
		MAP('V',                   IFEKEY_V);
		MAP('W',                   IFEKEY_W);
		MAP('X',                   IFEKEY_X);
		MAP('Y',                   IFEKEY_Y);
		MAP('Z',                   IFEKEY_Z);
		MAP('0',                   IFEKEY_0);
		MAP('1',                   IFEKEY_1);
		MAP('2',                   IFEKEY_2);
		MAP('3',                   IFEKEY_3);
		MAP('4',                   IFEKEY_4);
		MAP('5',                   IFEKEY_5);
		MAP('6',                   IFEKEY_6);
		MAP('7',                   IFEKEY_7);
		MAP('8',                   IFEKEY_8);
		MAP('9',                   IFEKEY_9);
		MAP(VK_OEM_COMMA,          IFEKEY_COMMA);
		MAP(VK_OEM_PERIOD,         IFEKEY_PERIOD);
		default: break;
	}
#undef MAP
}

void p_win32_ProcessKeyEvent(WPARAM w,LPARAM l,UINT msg,bool down) {
	IFEKeyEvent ke;

	if (w == VK_CONTROL || w == VK_LCONTROL || w == VK_RCONTROL ||
		w == VK_MENU || w == VK_LMENU || w == VK_RMENU ||
		w == VK_SHIFT || w == VK_LSHIFT || w == VK_RSHIFT ||
		w == VK_CAPITAL || w == VK_NUMLOCK) {
		UpdateWin32ModFlags();
	}

	/* NTS: This code intercepts WM_SYSKEYDOWN to catch the ALT key.
	 *      However if the user really does want to access the system menu,
	 *      we need to watch for VK_SPACE if ALT is down and then run
	 *      DefWindowProc() to make the system menu work.
	 *
	 *      Furthermore on Windows 3.1, we need to watch for VK_TAB
	 *      and ALT if the user wants to Alt+Tab away from our window.
	 *      Believe it or not, intercepting WM_SYSKEYDOWN is all it takes
	 *      to disable Alt+Tab in Windows 3.1. Later versions of Windows
	 *      will always allow Alt+Tab to work no matter what we intercept
	 *      for that reason. */
	if (win32_mod_flags & (IFEKeyEvent_FLAG_LALT|IFEKeyEvent_FLAG_RALT)) {
		/* If the shortcut is recognized, then call DefWindowProc() first with a fake
		 * event as if pressing the Alt key, then call DefWindowProc() with the current
		 * message.
		 *
		 * What this allows is the game to continue running even if the user merely
		 * taps the Alt key, but allows two well known Windows keyboard shortcuts to
		 * work properly. It also allows other Alt key combinations to be sent properly
		 * to the game.
		 *
		 * Did you know that in Windows 3.1, you can prevent the user from using Alt+Tab
		 * to switch away from your window if you intercept all WM_SYSKEYDOWN messages? */
		if (w == VK_SPACE) {
			IFEDBG("ALT+Space detected, Windows user is attempting to access system menu");
			DefWindowProc(hwndMain,WM_SYSKEYDOWN,VK_MENU,0x00000001u);
			DefWindowProc(hwndMain,msg,w,l);
		}
		else if (w == VK_TAB) {
			IFEDBG("ALT+Tab detected, Windows user is attempting to switch tasks");
			DefWindowProc(hwndMain,WM_SYSKEYDOWN,VK_MENU,0x00000001u);
			DefWindowProc(hwndMain,msg,w,l);
		}
	}

	/* lParam:
	 *   bits [15:0] = repeat count
	 *   bits [23:16] = scan code
	 *   bits [24:24] = extended key (right hand CTRL or ALT)
	 *   bits [30:30] = previous state
	 */

	/* NTS: Microsoft documents the low 16 bits as the repeat count, even in the Windows 3.1 SDK.
	 *      Problem is, that only works in later versions of Windows. In Windows 3.1, this
	 *      documentation is a big fat lie. In my testing, the low word is always 1, regardless
	 *      whether you just pushed the key, released the key, or are holding down the key.
	 *
	 *      However, we can filter out the repeated KEYDOWN messages by using instead the
	 *      "previous state" bit which does work as documented in Windows 3.1 */

	/* filter out repeated KEYDOWN events caused by holding the key down.
	 * this does not prevent WM_CHAR cooked character input from reflecting the repetition. */
	if (down && (l & 0x40000000ul)/*bit 30*/)
		return;

	memset(&ke,0,sizeof(ke));
	ke.raw_code = (uint32_t)w; /* raw code here is the VK_ code provided by Windows */
	ke.flags = (down ? IFEKeyEvent_FLAG_DOWN : 0) | win32_mod_flags;
	win32keyfill(ke,w,l);

	IFEDBG("Win32 key event: flags=%08x code=%08x w=%08x l=%08x",
		(unsigned int)ke.flags,
		(unsigned int)ke.code,
		(unsigned int)w,
		(unsigned int)l);

	if (!IFEKeyQueue.add(ke))
		IFEDBG("ProcessKeyboardEvent: Queue full");

	/* There is no need to do the translation to cooked, Windows provides it for us */
}

void p_win32_ProcessCharEvent(WPARAM w,LPARAM l) {
	(void)l;

	if (w != 0) {
		IFECookedKeyEvent cke;

		memset(&cke,0,sizeof(cke));
		cke.code = (uint32_t)w;

		IFEDBG("Cooked key: %c %d",cke.code >= 32 ? (char)cke.code : '?',(unsigned int)cke.code);

		if (!IFECookedKeyQueue.add(cke))
			IFEDBG("ProcessKeyboardEvent: Cooked queue full");
	}
}

LRESULT CALLBACK hwndMainProc(HWND hwnd,UINT uMsg,WPARAM wParam,LPARAM lParam) {
	switch (uMsg) {
		case WM_CREATE:
			break;
		case WM_QUIT:
			winQuit = true;
			break;
		case WM_DESTROY:
			if (!winIsDestroying)
				winQuit = true;
			break;
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
			/* NTS: LOWORD(lParam) returns a 16-bit integer. Mouse coordinates may be negative, therefore
			 *      typecast as signed 16-bit integer then sign extend to int */
			priv_ProcessMouseMotion(wParam,(int)((int16_t)LOWORD(lParam)),(int)((int16_t)HIWORD(lParam)));
			break;
		case WM_SYSKEYDOWN:/* or else this game is "hung" by DefWindowProc if the user taps the Alt key */
		case WM_KEYDOWN:
			p_win32_ProcessKeyEvent(wParam,lParam,uMsg,true);
			break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
			p_win32_ProcessKeyEvent(wParam,lParam,uMsg,false);
			break;
		case WM_CHAR:
			p_win32_ProcessCharEvent(wParam,lParam);
			break;
		case WM_PALETTECHANGED:
			if (winScreenIsPal && hwndMainPAL != NULL && (HWND)wParam != hwnd) {
				HPALETTE oldPal;
				HDC hDC;

				hDC = GetDC(hwnd);
				oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
				RealizePalette(hDC);
				SelectPalette(hDC,oldPal,FALSE);
				ReleaseDC(hwnd,hDC);
				InvalidateRect(hwnd,NULL,FALSE);
				return TRUE;
			}
			break;
		case WM_QUERYNEWPALETTE:
			if (winScreenIsPal && hwndMainPAL != NULL) {
				HPALETTE oldPal;
				HDC hDC;

				hDC = GetDC(hwnd);
				oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
				RealizePalette(hDC);
				SelectPalette(hDC,oldPal,FALSE);
				ReleaseDC(hwnd,hDC);
				InvalidateRect(hwnd,NULL,FALSE);
				return TRUE;
			}
			break;
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hDC = BeginPaint(hwnd,&ps);

				if (winScreenIsPal && hwndMainPAL != NULL) {
					HPALETTE oldPal;

					oldPal = SelectPalette(hDC,hwndMainPAL,FALSE);
					RealizePalette(hDC);
					SelectPalette(hDC,oldPal,FALSE);
				}

				EndPaint(hwnd,&ps);
				ifeapi->UpdateFullScreen();
			}
			break;
		case WM_ACTIVATE:
			UpdateWin32ModFlags();
			break;
		default:
			return DefWindowProc(hwnd,uMsg,wParam,lParam);
	}

	return 0;
}

bool priv_IFEWin32Init(HINSTANCE hInstance,HINSTANCE hPrevInstance/*doesn't mean anything in Win32*/,LPSTR lpCmdLine,int nCmdShow) {
	//not used yet
	(void)hInstance;
	(void)hPrevInstance;
	(void)lpCmdLine;
	(void)nCmdShow;

	cpu_probe();
	probe_dos();
	detect_windows();

	/* If Windows 3.1/95 (not Windows NT) then try to use the DOSBox Integration Device */
	if (windows_mode != WINDOWS_NT) {
		if (probe_dosbox_id()) {
			printf("DOSBox Integration Device detected\n");
			dosbox_ig = ifedbg_en = true;

			IFEDBG("Using DOSBox Integration Device for debug info. This should appear in your DOSBox/DOSBox-X log file");
		}
	}

	myInstance = hInstance;

	/* some performance and rendering improvements are possible if Windows 95 (aka Windows 4.0) or higher */
	if ((GetVersion()&0xFF) >= 4)
		win95 = true;
	else
		win95 = false;

	if (!hPrevInstance) {
		WNDCLASS wnd;

		memset(&wnd,0,sizeof(wnd));
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = hwndMainProc;
		wnd.hInstance = hInstance;
		wnd.lpszClassName = hwndMainClassName;

		if (!RegisterClass(&wnd)) {
			MessageBox(NULL,"RegisterClass failed","",MB_OK|MB_ICONEXCLAMATION);
			return false;
		}
	}

	return true;
}
#endif

