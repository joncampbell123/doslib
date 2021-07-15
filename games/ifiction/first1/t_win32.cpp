
#if defined(USE_WIN32)
# include <windows.h>
# include <mmsystem.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"
#include "palette.h"

#if defined(USE_WIN32)
extern bool		winScreenIsPal;
extern bool		winIsDestroying;
extern PALETTEENTRY	win_pal[256];
extern BITMAPINFO*	hwndMainDIB;
extern HPALETTE		hwndMainPAL;
extern HPALETTE		hwndMainPALPrev;
extern HWND		hwndMain;
extern DWORD		win32_tick_base;
extern unsigned char*	win_dib;
extern ifevidinfo_t	ifevidinfo_win32;
extern bool		winQuit;

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
	p_ShutdownVideo
};
#endif

