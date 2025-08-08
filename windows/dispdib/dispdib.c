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
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <windows/apihelp.h>
#include <windows/dispdib/dispdib.h>
#include <windows/win16eb/win16eb.h>

BITMAPINFOHEADER FAR*	dispDibFormat = NULL;
BYTE FAR*		dispDibBuffer = NULL;
HGLOBAL			dispDibBufferHandle = NULL;
DWORD			dispDibBufferFormat = 0;
DWORD			dispDibBufferSize = 0;

unsigned char		dispDibUseEntryPoints = 0; /* <- Under Windows 3.1, hwnd == NULL and this value nonzero */
HWND			dispDibHwnd = NULL; /* <-- NOTE: Under Windows 3.1, this is NULL, because we use the direct function calls */
HWND			dispDibHwndParent = NULL;
#if TARGET_MSDOS == 32
DWORD			dispDibDLLTask = 0;
DWORD			dispDibDLLWin16 = 0;
#else
HINSTANCE		dispDibDLL = NULL;
#endif
const char*		dispDibLoadMethod = "";
const char*		dispDibLastError = "";
unsigned char		dispDibUserActive = 0; /* User flag for caller to track whether it was active before switching away */
unsigned char		dispDibFullscreen = 0; /* whether we are fullscreen */
						/* ^ we need this flag because Windows 3.1 DISPDIB.DLL apparently treats all
						     BEGIN/END pairs with a counter. However many BEGINs you did, you need
						     just as many ENDs. we prefer not to do that. */
unsigned int		dispDibEnterCount = 0;
unsigned char		dispDibRedirectAllKeyboard = 0; /* If the calling app is passing all messages through us, redirect keyboard I/O to parent hwnd */
#if TARGET_MSDOS == 32
unsigned char		dispDibDontUseWin9xLoadLibrary16 = 0; /* calling app can force us not to use the Win9x/ME thunking method */
#endif

#if TARGET_MSDOS == 16
UINT (FAR PASCAL *__DisplayDib)(LPBITMAPINFOHEADER lpbi, LPSTR lpBits, WORD wFlags) = NULL;
UINT (FAR PASCAL *__DisplayDibEx)(LPBITMAPINFOHEADER lpbi, int x, int y, LPSTR lpBits, WORD wFlags) = NULL;
#endif

/* return TRUE if the VGA screen was configured (by DISPDIB.DLL) into Mode X 320x240 */
BYTE DisplayDibScreenPointerIsModeX() {
	return (dispDibFormat != NULL && dispDibFormat->biHeight > 200);
}

unsigned int DisplayDibScreenStride() {
	if (DisplayDibScreenPointerIsModeX())
		return 80;
	else
		return 320;
}

BYTE FAR *DisplayDibGetScreenPointerA000() {
#if TARGET_MSDOS == 16
	DWORD x = (DWORD)GetProcAddress(GetModuleHandle("KERNEL"),"__A000H");
	if (x == 0) return NULL;
	return (BYTE FAR*)MK_FP((unsigned int)(x&0xFFFF),0);
#else
	return (BYTE*)0xA0000;
#endif
}

BYTE FAR *DisplayDibGetScreenPointerB000() {
#if TARGET_MSDOS == 16
	DWORD x = (DWORD)GetProcAddress(GetModuleHandle("KERNEL"),"__B000H");
	if (x == 0) return NULL;
	return (BYTE FAR*)MK_FP((unsigned int)(x&0xFFFF),0);
#else
	return (BYTE*)0xB0000;
#endif
}

BYTE FAR *DisplayDibGetScreenPointerB800() {
#if TARGET_MSDOS == 16
	DWORD x = (DWORD)GetProcAddress(GetModuleHandle("KERNEL"),"__B800H");
	if (x == 0) return NULL;
	return (BYTE FAR*)MK_FP((unsigned int)(x&0xFFFF),0);
#else
	return (BYTE*)0xB8000;
#endif
}

int DisplayDibIsLoaded() {
#if TARGET_MSDOS == 32
	return (dispDibDLLTask != 0 || dispDibDLLWin16 != 0);
#else
	return (dispDibDLL != NULL);
#endif
}

int DisplayDibCreateWindow(HWND hwndParent) {
	if (dispDibHwnd != NULL || dispDibUseEntryPoints != 0)
		return 0;
	if (!DisplayDibIsLoaded())
		return 0;

#if TARGET_MSDOS == 32
	/* Windows 3.1, 32-bit: Don't bother, the DISPDIB.DLL implementation does not use messages.
	   Unfortunately the DLL entry points are 16-bit and we have no way to call them */
	/* TODO: Remove this check when we finish our DISPDIB.DLL "helper" program to emulate Win9x/ME messages */
	if (windows_version < ((3 << 8) + 95))
		return -1;
#else
	/* Windows 3.1, 16-bit: Don't create a window, the DISPDIB.DLL implementation there is not Window message based like Win9x/ME,
	   instead we direct DISPDIB.DLL by calling specific entry points */
	if (windows_version < ((3 << 8) + 95)) {
		if (__DisplayDib == NULL || __DisplayDibEx == NULL)
			return -1;

		dispDibHwndParent = hwndParent;
		dispDibUseEntryPoints = 1;
		return 0;
	}
#endif

	dispDibHwndParent = hwndParent;
#if TARGET_MSDOS == 32
	dispDibHwnd = CreateWindow(DISPLAYDIB_WINDOW_CLASS,"",WS_POPUP,0,0,
		GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),hwndParent,NULL,
		(HINSTANCE)GetWindowLong(hwndParent,GWL_HINSTANCE),
		NULL);
#else
	dispDibHwnd = CreateWindow(DISPLAYDIB_WINDOW_CLASS,"",WS_POPUP,0,0,
		GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),hwndParent,NULL,
		(HINSTANCE)GetWindowWord(hwndParent,GWW_HINSTANCE),
		NULL);
#endif

	if (dispDibHwnd == NULL) {
#if TARGET_MSDOS == 32
		dispDibHwnd = CreateWindow(DISPLAYDIB_WINDOW_CLASS_CAPS,"",WS_POPUP,0,0,
			GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),hwndParent,NULL,
			(HINSTANCE)GetWindowLong(hwndParent,GWL_HINSTANCE),
			NULL);
#else
		dispDibHwnd = CreateWindow(DISPLAYDIB_WINDOW_CLASS_CAPS,"",WS_POPUP,0,0,
			GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),hwndParent,NULL,
			(HINSTANCE)GetWindowWord(hwndParent,GWW_HINSTANCE),
			NULL);
#endif
	}

	if (dispDibHwnd == NULL)
		return -1;

	return 0;
}

int DisplayDibAllocBuffer() {
	if (dispDibBufferHandle == NULL)
		dispDibBufferHandle = GlobalAlloc(GMEM_MOVEABLE,dispDibBufferSize);

	if (dispDibBufferHandle == NULL) {
		char tmp[64];
		sprintf(tmp,"%lu bytes",(unsigned long)dispDibBufferSize);
		MessageBox(NULL,tmp,"Could not allocate",MB_OK);
	}

	return (dispDibBufferHandle != NULL) ? 0 : -1;
}

BYTE FAR *DisplayDibLockBuffer() {
	if (dispDibBuffer == NULL) 
		dispDibBuffer = GlobalLock(dispDibBufferHandle);

	return dispDibBuffer;
}

void DisplayDibUnlockBuffer() {
	if (dispDibBuffer != NULL) {
		GlobalUnlock(dispDibBufferHandle);
		dispDibBuffer = NULL;
	}
}

void DisplayDibFreeBuffer() {
	DisplayDibUnlockBuffer();
	if (dispDibBufferHandle != NULL) GlobalFree(dispDibBufferHandle);
	dispDibBufferHandle = NULL;
}

RGBQUAD FAR *DisplayDibPalette() {
#if TARGET_MSDOS == 32
	return (RGBQUAD*)(((BYTE*)dispDibFormat) + sizeof(BITMAPINFOHEADER));
#else
	return (RGBQUAD FAR*)(((BYTE FAR*)dispDibFormat) + sizeof(BITMAPINFOHEADER));
#endif
}

int DisplayDibSetFormat(unsigned int width,unsigned int height) {
	if (dispDibFormat != NULL) {
		if (dispDibFormat->biWidth == width && dispDibFormat->biHeight == height)
			return 0;
	}

	/* free buffer */
	DisplayDibUnlockBuffer();
	DisplayDibFreeBuffer();

	/* free format */
	if (dispDibFormat != NULL) {
#if TARGET_MSDOS == 32
		free(dispDibFormat);
#else
		_ffree(dispDibFormat);
#endif
		dispDibFormat = NULL;
	}

	/* if width == 0 || height == 0 (caller freeing) then exit now */
	if (width == 0 || height == 0)
		return 0;

#if TARGET_MSDOS == 32
	dispDibFormat = malloc(sizeof(BITMAPINFOHEADER) + (256*sizeof(RGBQUAD)));
#else
	dispDibFormat = _fmalloc(sizeof(BITMAPINFOHEADER) + (256*sizeof(RGBQUAD)));
#endif
	if (dispDibFormat == NULL)
		return -1;

	dispDibFormat->biSize = sizeof(BITMAPINFOHEADER);
	/* NTS: DISPDIB.DLL renders it "bottom up" like any standard bitmap. It does include code to support
	   negative heights like -240 for "top down" but as far as I can tell, it doesn't work properly
	   and can often cause crashes in Windows 98's DISPDIB.DLL.

	   I recommend that if you want top-down rendering you exploit Windows 9x's permissiveness and
	   just typecast a pointer to 0xA0000.

	   Note that DISPDIB.DLL always programs 320x240 into "Mode X", so you will have to play with
	   the VGA registers to work with it */
	dispDibFormat->biWidth = width;
	dispDibFormat->biHeight = height;
	dispDibFormat->biPlanes = 1;
	dispDibFormat->biBitCount = 8;
	dispDibFormat->biCompression = 0;
	dispDibFormat->biSizeImage = (DWORD)abs(dispDibFormat->biWidth) * (DWORD)abs(dispDibFormat->biHeight);
	dispDibFormat->biXPelsPerMeter = 0;
	dispDibFormat->biYPelsPerMeter = 0;
	dispDibFormat->biClrUsed = 256;
	dispDibFormat->biClrImportant = 256;
	dispDibBuffer = NULL;
	dispDibBufferSize = dispDibFormat->biSizeImage;
	DisplayDibAllocBuffer();
	return 0;
}

int DisplayDibGenerateTestImage() {
	RGBQUAD FAR *pal;
	unsigned int i,x,y;
	unsigned long ofs;

	if (dispDibFormat == NULL)
		return -1;

	pal = DisplayDibPalette();
	for (i=0;i < 64;i++) {
		pal[i].rgbBlue = pal[i].rgbGreen = pal[i].rgbRed = i*4;
		pal[i].rgbReserved = 0;
	}
	for (i=0;i < 64;i++) {
		pal[i+64].rgbRed = i*4;
		pal[i+64].rgbGreen = 0;
		pal[i+64].rgbBlue = 0;
		pal[i+64].rgbReserved = 0;
	}
	for (i=0;i < 64;i++) {
		pal[i+128].rgbRed = 0;
		pal[i+128].rgbGreen = i*4;
		pal[i+128].rgbBlue = 0;
		pal[i+128].rgbReserved = 0;
	}
	for (i=0;i < 64;i++) {
		pal[i+192].rgbRed = 0;
		pal[i+192].rgbGreen = 0;
		pal[i+192].rgbBlue = i*4;
		pal[i+192].rgbReserved = 0;
	}

	if (DisplayDibLockBuffer() != NULL) {
		/* Win16 buffer explanation: To understand what the hell I'm doing here, you have to understand
		   how Microsoft implemented Win16 to support buffers larger than 64KB.

		   In normal circumstances, a GlobalLock() request returns one segment value that covers up to
		   64KB of memory. But if you used GlobalAlloc() to allocate >= 64KB, then GlobalLock() allocates
		   multiple selectors in sequence and returns the first one. You're supposed to know at this point
		   that the first 64KB is accessible from the first selector, the next 64KB from the next selector,
		   etc.. and so on.

		   Now, because of programming details related to selector values in the i386 world, the selector
		   segment values are not 1 apart, but 8 apart. If the first selector is 0x3B, then the next one is
		   0x3B + 8 = 0x43.

		   Hopefully that should clear up the weird pointer math I'm doing here for Win16 builds :) */
		/* FIXME: Does GlobalAlloc always return MK_FP(segment,0) or must we anticipate the possibility
			  that the offset portion is nonzero? */
		for (y=0;y < (dispDibFormat->biHeight/2);y++) {
			for (x=0;x < dispDibFormat->biWidth;x++) {
				ofs = ((unsigned long)y * (unsigned long)abs(dispDibFormat->biWidth)) +
					(unsigned long)x
#if TARGET_MSDOS == 16
					+ (unsigned long)FP_OFF(dispDibBuffer)
#endif
					;

#if TARGET_MSDOS == 32
				dispDibBuffer[(unsigned int)ofs] = 
#else
				*((BYTE FAR*)MK_FP(FP_SEG(dispDibBuffer)+((ofs>>16)*8),(unsigned int)ofs)) =
#endif
					x&0xFF;
			}
		}
		for (;y < dispDibFormat->biHeight;y++) {
			for (x=0;x < dispDibFormat->biWidth;x++) {
				ofs = ((unsigned long)y * (unsigned long)abs(dispDibFormat->biWidth)) +
					(unsigned long)x
#if TARGET_MSDOS == 16
					+ (unsigned long)FP_OFF(dispDibBuffer)
#endif
					;

#if TARGET_MSDOS == 32
				dispDibBuffer[(unsigned int)ofs] = 
#else
				*((BYTE FAR*)MK_FP(FP_SEG(dispDibBuffer)+((ofs>>16)*8),(unsigned int)ofs)) =
#endif
					(x^y)&0xFF;
			}
		}
		DisplayDibUnlockBuffer();
	}

	return 0;
}

int DisplayDibLoadDLL() {
	UINT oldMode;

	/* take into consideration the host OS. Windows 9x/ME basically allows LoadLibrary (Win16) or LoadModule (Win32).
	   getting this to work with Windows 3.1 however requires some extra work, because the Windows 3.1 version of
	   DISPDIB.DLL doesn't register a window class at all.

	   Note that since DISPDIB.DLL is 16-bit, and the Windows 3.1 version doesn't register a window class, there's
	   no way for Win32s applications to directly use the library. Windows 9x/ME DISPDIB.DLL however does register
	   a window class, which by design of the message pump system, allows Win32 applications to use it too (mostly
	   through WM_COPYDATA).

	   TODO: It might be a nice hack for Windows 3.1 Win32s if we wrote our own "helper" Win16 EXE that would wrap
		 DISPDIB.DLL and emulate Windows 95-style DISPDIB messages with it. We know enough about the segmented
		 memory model to even do a proper WM_COPYDATA implementation for it :) */
	probe_dos();
	detect_windows();

	if (DisplayDibIsLoaded())
		return 0;

	/* This will not work under Windows NT/2000/XP/etc. They lack DISPDIB.DLL anyway */
	/* FIXME: Actually there is supposedly a version of DISPDIB for Windows NT/2000 that emulates this DLL, is that true? */
	if (windows_mode == WINDOWS_NT)
		{ dispDibLastError="Not supported under Windows NT"; return -1; }
	/* do NOT attempt while under Linux + WINE */
	if (windows_emulation == WINEMU_WINE)
		{ dispDibLastError="Not supported under WINE"; return -1; }

#if TARGET_MSDOS == 32
	{
		const char *what = DISPLAYDIB_DLL;
		DWORD show = 2,zero = 0;
		LPVOID params[4];

		/* Windows 3.1 + Win32s: Don't try, it won't work. Windows 3.1 DISPDIB.DLL lacks the window class
		   and message-based design that Windows 9x/ME uses to allow Win32 application to use the library */
		if (windows_version < ((3 << 8) + 95))
			{ dispDibLastError="Not supported under Win32s"; return -1; }

		/* Use the Win9x/ME thunk library if available */
		if (!dispDibDontUseWin9xLoadLibrary16)
			Win9xQT_ThunkInit();

		/* Some functions require the Win16 "executable buffer" library */
		InitWin16EB();

		/* Method "A": Use the Win9x/ME thunk library to load DISPDIB.DLL as a Win16 DLL.
		   This is NOT sanctioned by Microsoft, but gives us a reference to the DLL in a way we can later
		   free the DLL when we're done. If desired, it also opens up the possibility of using the DisplayDib()
		   functions it provides directly.

		   In my opinion this is a much cleaner way to make use of the library, compared to the more common
		   Method "B" below */
		if (!DisplayDibIsLoaded() && !dispDibDontUseWin9xLoadLibrary16 && LoadLibrary16 != NULL && FreeLibrary16 != NULL && windows_mode != WINDOWS_NT) {
			dispDibDLLWin16 = LoadLibrary16((LPSTR)what);
			if (dispDibDLLWin16 != 0) dispDibLoadMethod = "Win9x/ME thunk LoadLibrary16";
		}

		/* Method "B": Use LoadModule() to start it up. This according to DISPDIB.H is the official Microsoft
		   approved method of using DISPDIB.DLL from a Win32 application. Unfortunately LoadModule() also returns
		   a bogus handle for compatability and therefore we have no way to free the module when we're done.

		   Since this method is used by virtually every Win9x game that goes fullscreen 320x200, DISPDIB.DLL
		   never really unloads from the system. */
		if (!DisplayDibIsLoaded()) {
			params[0] = NULL;
			params[1] = &zero;
			params[2] = &show;
			params[3] = 0;
			oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
			dispDibDLLTask = LoadModule(what,&params);
			SetErrorMode(oldMode);
			if (dispDibDLLTask < (DWORD)HINSTANCE_ERROR) dispDibDLLTask = 0;
			else dispDibLoadMethod = "LoadModule()";
		}

		if (!DisplayDibIsLoaded())
			return -1;
	}
#else
	/* FIXME: Did DISPDIB.DLL appear in Windows 3.1? Or did it also appear in Windows 3.0 Multimedia Extensions? */
	oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
	dispDibDLL = LoadLibrary(DISPLAYDIB_DLL);
	SetErrorMode(oldMode);
	if (dispDibDLL < (HINSTANCE)HINSTANCE_ERROR) {
		dispDibLastError = "Failed to load DISPDIB";
		dispDibDLL = NULL;
		return -1;
	}
	dispDibLoadMethod = "LoadLibrary";

	__DisplayDib = (UINT (FAR PASCAL *)(LPBITMAPINFOHEADER,LPSTR,WORD))GetProcAddress(dispDibDLL,"DISPLAYDIB");
	__DisplayDibEx = (UINT (FAR PASCAL *)(LPBITMAPINFOHEADER,int,int,LPSTR,WORD))GetProcAddress(dispDibDLL,"DISPLAYDIBEX");
#endif

	DisplayDibSetFormat(320,200);
	return 0;
}

/* caller must have locked the buffer and obtained the buffer ptr.
   this is only for Win16 Windows 3.1 who lacks 320x400 draw routines */
void DisplayDibDrawModeX320x400() {
	/* TODO */
}

/* caller must have locked the buffer and obtained the buffer ptr.
   this is only for Win16 Windows 3.1 who lacks 320x400 draw routines */
void DisplayDibDrawModeX320x480() {
	/* TODO */
}

void DisplayDibDoUpdate() {
	if (dispDibFormat == NULL)
		return;
	if (dispDibBufferHandle == NULL)
		return;

	DisplayDibLockBuffer();

	if (dispDibHwnd != NULL)
		DisplayDibWindowDraw(dispDibHwnd,0,dispDibBuffer,dispDibBufferSize);
#if TARGET_MSDOS == 16
	else if (dispDibUseEntryPoints) {
		/* Windows 3.1 DISPDIB.DLL does not support 320x400 and 320x480 so we have to draw it ourself */
		if (abs((int)(dispDibFormat->biHeight)) >= 480) {
			DisplayDibDrawModeX320x480();
			__DisplayDib(dispDibFormat,dispDibBuffer,dispDibBufferFormat|DISPLAYDIB_DONTLOCKTASK|DISPLAYDIB_NOIMAGE);
		}
		else if (abs((int)(dispDibFormat->biHeight)) >= 400) {
			DisplayDibDrawModeX320x400();
			__DisplayDib(dispDibFormat,dispDibBuffer,dispDibBufferFormat|DISPLAYDIB_DONTLOCKTASK|DISPLAYDIB_NOIMAGE);
		}
		else {
			__DisplayDib(dispDibFormat,dispDibBuffer,dispDibBufferFormat|DISPLAYDIB_DONTLOCKTASK);
		}
	}
#endif

	DisplayDibUnlockBuffer();
}

void DisplayDibDoUpdatePalette() {
	if (dispDibFormat == NULL)
		return;
	if (dispDibBufferHandle == NULL)
		return;

	DisplayDibLockBuffer();

	if (dispDibHwnd != NULL)
		DisplayDibWindowSetFmt(dispDibHwnd,dispDibFormat);
#if TARGET_MSDOS == 16
	else if (dispDibUseEntryPoints)
		__DisplayDib(dispDibFormat,dispDibBuffer,dispDibBufferFormat|DISPLAYDIB_NOIMAGE|DISPLAYDIB_DONTLOCKTASK);
#endif

	DisplayDibUnlockBuffer();
}

int DisplayDibDoBegin() {
	if (dispDibFullscreen)
		return 0;
	if (dispDibFormat == NULL)
		{ dispDibLastError="DibFormat == NULL"; return -1; }
	if (dispDibBufferHandle == NULL)
		{ dispDibLastError="DibBufferHandle == NULL"; return -1; }

	if (DisplayDibLockBuffer() == NULL)
		{ dispDibLastError="Failed to lock buffer"; return -1; }

	if (dispDibHwnd != NULL) {
		if (DisplayDibWindowSetFmt(dispDibHwnd,dispDibFormat) != 0) {
			dispDibLastError="Set format failed";
			goto fail;
		}
		if (DisplayDibWindowBeginEx(dispDibHwnd) != 0) {
			dispDibLastError="Begin failed";
			goto fail;
		}
	}
#if TARGET_MSDOS == 16
	else if (dispDibUseEntryPoints) {
		if (dispDibFormat->biWidth != 320) goto fail;

		/* While Windows 95/98/MX DISPDIB.DLL supports the 320x400 and 320x480 modes, Windows 3.1 DISPDIB.DLL doesn't.
		   But since Windows 3.1 leaves the VGA I/O ports open, we can program it in anyway */
		if (abs((int)(dispDibFormat->biHeight)) == 200 || abs((int)(dispDibFormat->biHeight)) == 400)
			dispDibBufferFormat = DISPLAYDIB_MODE_320x200x8;
		else if (abs((int)(dispDibFormat->biHeight)) == 240 || abs((int)(dispDibFormat->biHeight)) == 480)
			dispDibBufferFormat = DISPLAYDIB_MODE_320x240x8;
		else
			goto fail;

		/* NTS: You *HAVE* to specify DISPLAYDIB_DONTLOCKTASK. If you don't, the Windows 3.1 version of DISPDIB.DLL
		        will "lock" this task, which apparently means that upon DISPLAYDIB_BEGIN, this program
			is mysteriously locked out of receiving mouse and keyboard input. What the fuck Microsoft?

			Windows 95 and higher don't seem to have this problem. */
		if (__DisplayDib(NULL,NULL,DISPLAYDIB_BEGIN|dispDibBufferFormat|DISPLAYDIB_NOWAIT|DISPLAYDIB_DONTLOCKTASK) != 0) goto fail;

		/* 320x400/480 mode: we have to modify the VGA registers a bit */
		if (abs((int)(dispDibFormat->biHeight)) >= 400) {
			outp(0x3C4,0x04); outp(0x3C5,0x06);
			outp(0x3C4,0x00); outp(0x3C5,0x01);
			outp(0x3D4,0x17); outp(0x3D5,0xE3);
			outp(0x3D4,0x14); outp(0x3D5,0x00);
			outp(0x3C4,0x00); outp(0x3C5,0x03);
			outp(0x3C4,0x02); outp(0x3C5,0x0F);
			outp(0x3D4,0x09); outp(0x3D5,0x00);
		}
	}
#endif

	dispDibRedirectAllKeyboard = 1;
	dispDibEnterCount++;
	dispDibFullscreen = 1;
	DisplayDibUnlockBuffer();
	DisplayDibDoUpdate();
	dispDibLastError = "";
	return 0;
fail:	DisplayDibUnlockBuffer();
	return -1;
}

int DisplayDibCheckMessageAndRedirect(LPMSG msg) {
	if (dispDibRedirectAllKeyboard && dispDibHwndParent != NULL) {
		if (msg->message == WM_KEYUP || msg->message == WM_KEYDOWN || msg->message == WM_CHAR) {
			msg->hwnd = dispDibHwndParent;
			return 1;
		}
	}

	/* Windows 3.1 hack: System or user menus popping up while fullscreen cause a crash, apparently.
	   For everyone else, it's probably a nice gesture as well since it gives the program something to do with the ALT key */
	if (dispDibFullscreen) {
		if (msg->message == WM_INITMENUPOPUP || msg->message == WM_SYSKEYDOWN || msg->message == WM_SYSKEYUP || msg->message == WM_SYSCHAR)
			return -1;
	}

	return 0;
}

int DisplayDibDoEnd() {
	if (dispDibFullscreen == 0)
		return 0;

	if (dispDibHwnd != NULL) {
		if (DisplayDibWindowEndEx(dispDibHwnd) != 0) return -1;
	}
#if TARGET_MSDOS == 16
	else if (dispDibUseEntryPoints) {
		if (__DisplayDib(NULL,NULL,DISPLAYDIB_END|DISPLAYDIB_NOWAIT|DISPLAYDIB_DONTLOCKTASK) != 0) return -1;
	}
#endif

	dispDibRedirectAllKeyboard = 0;
	dispDibFullscreen = 0;
	return 0;
}

int DisplayDibUnloadDLL() {
	DisplayDibDoEnd();
	DisplayDibFreeBuffer();

	if (dispDibFormat != NULL) {
#if TARGET_MSDOS == 32
		free(dispDibFormat);
#else
		_ffree(dispDibFormat);
#endif
		dispDibFormat = NULL;
	}

	if (dispDibHwnd != NULL) {
		/* DDM_CLOSE makes the window close itself, the DISPDIB.DLL free itself. */
		/* But here's the catch Microsoft's fancy code forgot: what *IF* it loaded the DLL but failed to create the window---then what?
		   So our job here is to do the ideal task assuming we got a window, or else leave it be for the more brutal freeup code below. */
		DisplayDibWindowClose(dispDibHwnd);
		dispDibHwnd = NULL;

		/* OK, DDM_CLOSE is supposed to cause the DLL to close the window and release the DLL.
		   But that doesn't seem to happen under Windows 9x/ME, so we have to release it ourself */
		if (windows_version < ((3 << 8) + 95)) { /* Prior to Windows 95 */
#if TARGET_MSDOS == 32
			dispDibDLLTask = 0;
			dispDibDLLWin16 = 0;
#else
			dispDibDLL = NULL;
#endif
		}
	}

#if TARGET_MSDOS == 32
	/* If we loaded it by LoadModule(), then as far as Win9x/ME mechanics go, there's nothing we can do to "free it".
	   It just floats out there. If DISPDIB.H in Microsoft SDK is any indication, that's how it's supposed to work,
	   because that standard code makes no attempt to free it (which raises the question: does LoadModule() increment
	   a reference count on it causing a resource leak, or just refer to it again?)

	   If we loaded it directly, then we can free it and free up system resources.
	   Note that the concurrent nature of Win9x/ME means HEAPWALK.EXE might refresh and still show a DISPDIB.DLL, but
	   another refresh will see it disappear with a little delay.

	   This works perfectly fine in Windows 95, 98, and ME */
	if (dispDibDLLWin16 != 0 && FreeLibrary16 != NULL) FreeLibrary16(dispDibDLLWin16);
	dispDibDLLWin16 = 0;
	dispDibDLLTask = 0;
#else
	if (dispDibDLL != NULL) {
		FreeLibrary(dispDibDLL);
		dispDibDLL = NULL;
	}
	__DisplayDib = NULL;
	__DisplayDibEx = NULL;
#endif
	dispDibUseEntryPoints = 0;
	return 0;
}

int DisplayDibSetBIOSVideoMode(unsigned char x) {
#if TARGET_MSDOS == 16
	/* Believe it or not----THIS WORKS. Win16 apps can directly do INT 10h */
	__asm {
		mov	al,x
		xor	ah,ah
		int	10h
	}
#else
	if (!Win16EBavailable) return 0;

	/* Win32 apps cannot directly do an INT 10h, it seems to cause a BSOD.
	   But if Win16 Executable Buffers are available, we can write an INT 10h
	   call routine there and do it! */
	{
		BYTE FAR *p;
		DWORD code;

		if ((p=LockWin16EBbuffer()) == NULL) return 0;

		/* force creation of the code alias */
		code = GetCodeAliasWin16EBbuffer();
		if (code == 0) {
			UnlockWin16EBbuffer();
			return 0;
		}

		/* MOV AX,<x> */
		*p++ = 0xB8; *((WORD FAR*)p) = x&0xFF; p+=2;
		/* INT 10h */
		*p++ = 0xCD; *p++ = 0x10;
		/* RETF */
		*p++ = 0xCB;

		ExecuteWin16EBbuffer(code);
		UnlockWin16EBbuffer();
	}
#endif

	return 1;
}

