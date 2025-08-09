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

#include <sys/types.h>
#include <sys/stat.h>
#include <windows.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <i86.h>
#include <dos.h>
#include "resource.h"

#include <windows/apihelp.h>

static char hexes[] = "0123456789ABCDEF";

static char *uint2hex(char *w,unsigned int v,unsigned int digits) {
	char *r = w + digits;

	while (digits > 0u) {
		w[--digits] = hexes[v&0xFu];
		v >>= 4u;
	}

	*r = 0;
	return r;
}

// copy pasts from hw/dos until hw/dos can compile for Windows 1.x
#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
# if WINVER >= 0x200
static DWORD Win16_KERNELSYM(const unsigned int ord) {
	HMODULE krnl = GetModuleHandle("KERNEL");
	if (krnl) return (DWORD)GetProcAddress(krnl,MAKEINTRESOURCE(ord));
	return 0;
}
# endif

static unsigned int Win16_AHINCR(void) {
# if WINVER >= 0x200
	return (unsigned)(Win16_KERNELSYM(114) & 0xFFFFu);
# else
	return 0x1000u; /* Windows 1.x is real mode only, assume real mode __AHINCR */
# endif
}
#endif // 16-bit Windows

HWND near			hwndMain;
const char near			WndProcClass[] = "CLIPBRD1WINDOWS";
HINSTANCE near			myInstance;

HWND near			cbListHwnd = NULL;

HWND near			cbViewNextHwnd = NULL;
BOOL near			cbViewInit = FALSE;

struct clipboard_fmt_t {
	UINT			cbFmt;
};

#define MAX_CLIPBOARD_FORMATS 256
static struct clipboard_fmt_t near clipboard_format[MAX_CLIPBOARD_FORMATS];
static unsigned int near clipboard_format_count = 0;

static char *MakeCFName(char *w,char *f,UINT efmt,BOOL isFileName) {
	int r;

	switch (efmt) {
		case CF_BITMAP:
			if (isFileName) { strcpy(w,"BITMAP"); break; }
			strcpy(w,"CF_BITMAP"); break;
		case CF_DIB:
			if (isFileName) { strcpy(w,"DIB"); break; }
			strcpy(w,"CF_DIB"); break;
		case CF_DIF:
			if (isFileName) { strcpy(w,"DIF"); break; }
			strcpy(w,"CF_DIF"); break;
		case CF_DSPBITMAP:
			if (isFileName) { strcpy(w,"DSPBMP"); break; }
			strcpy(w,"CF_DSPBITMAP"); break;
		case CF_DSPMETAFILEPICT:
			if (isFileName) { strcpy(w,"DSPMFP"); break; }
			strcpy(w,"CF_DSPMETAFILEPICT"); break;
		case CF_DSPTEXT:
			if (isFileName) { strcpy(w,"DSPTEXT"); break; }
			strcpy(w,"CF_DSPTEXT"); break;
		case CF_METAFILEPICT:
			if (isFileName) { strcpy(w,"METAFPCT"); break; }
			strcpy(w,"CF_METAFILEPICT"); break;
		case CF_OEMTEXT:
			if (isFileName) { strcpy(w,"OEMTEXT"); break; }
			strcpy(w,"CF_OEMTEXT"); break;
		case CF_OWNERDISPLAY:
			if (isFileName) { strcpy(w,"OWNDISP"); break; }
			strcpy(w,"CF_OWNERDISPLAY"); break;
		case CF_PALETTE:
			if (isFileName) { strcpy(w,"PALETTE"); break; }
			strcpy(w,"CF_PALETTE"); break;
		case CF_PENDATA:
			if (isFileName) { strcpy(w,"PENDATA"); break; }
			strcpy(w,"CF_PENDATA"); break;
		case CF_RIFF:
			if (isFileName) { strcpy(w,"RIFF"); break; }
			strcpy(w,"CF_RIFF"); break;
		case CF_SYLK:
			if (isFileName) { strcpy(w,"SYLK"); break; }
			strcpy(w,"CF_SYLK"); break;
		case CF_TEXT:
			if (isFileName) { strcpy(w,"TEXT"); break; }
			strcpy(w,"CF_TEXT"); break;
		case CF_TIFF:
			if (isFileName) { strcpy(w,"TIFF"); break; }
			strcpy(w,"CF_TIFF"); break;
		case CF_WAVE:
			if (isFileName) { strcpy(w,"WAVE"); break; }
			strcpy(w,"CF_WAVE"); break;
		default:
			if (efmt >= 0xC000/*registered*/) {
				r = GetClipboardFormatName(efmt,w,(int)(f-w));
				if (r < 0) r = 0;
				w += r; if (w > f) w = f;
				*w = 0;
			}
			else {
				*w++ = '0';
				*w++ = 'x';
				w = uint2hex(w,efmt,4);
			}
			break;
	}

	return w;
}

#if TARGET_MSDOS == 16
static void DumpToFile(int fd,void FAR *p,DWORD psz) {
#else
static void DumpToFile(int fd,void *p,DWORD psz) {
#endif
	if (p) {
#if TARGET_MSDOS == 16
		/* NTS: Watcom C does not help us here and does not provide FAR pointer math when adding to p,
		 *      so we have to FAR pointer math ourself, manually. */
		const unsigned int AHINCR = Win16_AHINCR();
		unsigned int sv = FP_SEG(p),ov = FP_OFF(p);
		const unsigned int blksz = 0x4000u;
		unsigned int todo;
		DWORD rem = psz;

		while (rem != (DWORD)0) {
			if (ov > (0x10000u - blksz))
				todo = 0x10000u - ov; /* from ov to end of 64KB segment */
			else
				todo = blksz; /* blksz */

			if ((DWORD)todo > rem)
				todo = (unsigned int)rem;

			write(fd,MK_FP(sv,ov),todo);
			rem -= (DWORD)todo;

			/* the above math should ensure that (ov += todo) < 0x10000 or (ov += todo) == 0 */
			if ((unsigned int)(ov += todo) == 0)
				sv += AHINCR; // TODO: Use Windows __AHINCR, in case we're running in real mode
		}
#else
		if (psz) write(fd,p,psz);
#endif
	}
}

static void DumpBitmapBits(int fd,BITMAP *bm,HBITMAP han) {
	DWORD bmsz = (DWORD)bm->bmHeight * (DWORD)bm->bmWidthBytes * (DWORD)bm->bmPlanes;
#if TARGET_MSDOS == 16
	void FAR *p;
#else
	void *p;
#endif

	if (bmsz) {
		HGLOBAL tmp = GlobalAlloc(GHND,bmsz);
		if (tmp) {
			p = GlobalLock(tmp);
			if (p) {
				GetBitmapBits((HBITMAP)han,bmsz,p);
				DumpToFile(fd,p,bmsz);
				GlobalUnlock(tmp);
			}
			GlobalFree(tmp);
		}
	}
}

static void DoClipboardSaveFormat(unsigned int sel) {
	UINT efmt = clipboard_format[sel].cbFmt;
	char fname[128];
	unsigned int x;
	HANDLE han;
	int fd;

	fname[0] = 0;
	MakeCFName(fname,fname+sizeof(fname)-1,efmt,TRUE);
	x = strlen(fname);
	if (x > 8) x = 8;
	if (x == 0) fname[x++] = '_';
	strcpy(fname+x,".bin");

	han = GetClipboardData(efmt);
	if (han) {
#if TARGET_MSDOS == 16
		// FIXME: open() in real-mode Windows is unable to open and create a file if it doesn't exist??
		{
			int han = 0;
			if (_dos_creat(fname,0,&han) == 0) _dos_close(han);
		}
#endif
		fd = open(fname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
		if (fd >= 0) {
			if (efmt == CF_BITMAP) {
				BITMAP bm;

				memset(&bm,0,sizeof(bm));
				if (GetObject((HGDIOBJ)han,sizeof(bm),&bm) == sizeof(bm)) {
					write(fd,&bm,sizeof(bm));
					DumpBitmapBits(fd,&bm,(HBITMAP)han);
				}
			}
			else if (efmt == CF_PALETTE) {
				int colors = 0;

				if (GetObject((HGDIOBJ)han,sizeof(int),&colors) == sizeof(int)) {
					if (colors > 0 && colors <= 256) {
						PALETTEENTRY *lp = malloc(sizeof(PALETTEENTRY) * colors);
						if (lp) {
							memset(lp,0,sizeof(PALETTEENTRY) * colors);
							GetPaletteEntries((HPALETTE)han,0,colors,lp);
							write(fd,lp,sizeof(PALETTEENTRY) * colors);
							free(lp);
						}
					}
				}
			}
			else {
				DWORD psz = GlobalSize((HGLOBAL)han);
#if TARGET_MSDOS == 16
				void FAR *p = GlobalLock((HGLOBAL)han);
#else
				void *p = GlobalLock((HGLOBAL)han);
#endif
				DumpToFile(fd,p,psz);
				GlobalUnlock((HGLOBAL)han);
			}
			close(fd);
		}
	}
}

static unsigned int bitmap_stride_from_bpp_and_w(unsigned int bpp,unsigned int w) {
	if (bpp == 15) bpp = 16;
	return (((w * bpp) + 31u) & (~31u)) >> 3u;
}

#if WINVER >= 0x300
static HPALETTE CreateIdentityPalette(const unsigned int colors) {
	unsigned int i;
	LOGPALETTE *pal = NULL;
	HPALETTE ph = (HPALETTE)NULL;

	pal = malloc(sizeof(LOGPALETTE) + (colors * sizeof(PALETTEENTRY)));
	if (pal) {
		pal->palVersion = 0x300;
		pal->palNumEntries = colors;

		for (i=0;i < colors;i++) {
			/* red,green,blue,flags, first 16 bits span red-green */
			*((WORD*)(&(pal->palPalEntry[i].peRed))) = i;
			pal->palPalEntry[i].peBlue = 0;
			pal->palPalEntry[i].peFlags = PC_EXPLICIT;
		}

		ph = CreatePalette(pal);
		if (!ph) MessageBeep(-1);
		free(pal);
	}

	return ph;
}
#endif

static void DumpBITMAPasBMP(void) {
// NOTES: Windows 3.0 and higher have the Print Screen function, which produces CF_BITMAP.
//        On a 256-color display, Print Screen produces only CF_BITMAP on Windows 3.0 which means
//        if the palette changes afterward, anything outside the standard 20 colors are "corrupted".
//        Windows 3.1 puts CF_BITMAP and CF_PALETTE so that the palette is presevved.
//
//        Windows 3.0: If the DDB in CF_BITMAP is planar such as 16-color VGA, we can use
//        GetDIBits instead to convert it to a format appropriate for a BMP file. Windows 2.x
//        and 1.x as far as I can tell will never provide a user a way to copy a color bitmap
//        to the clipboard, if that happens, we're in trouble because GetDIBits and DIB API
//        functions did not exist until 3.0, but again, monochrome bitmaps are everywhere as
//        a sort of common baseline and it is unlikely for any user-visible tool to allow them
//        to make and copy color bitmaps, so, no big deal really.
//
//        If somehow in Windows 1.x and 2.x a color CF_BITMAP is on the screen, we cannot render
//        it because those are device driver specific, especially planar graphics mode. DIBs
//        didn't exist back then. Do you understand now why bitmaps and icons in that time
//        were always 1bpp monochrome? It's a safe common ground for all code.
//
//        Windows 2.x amd 1.x do not have a print screen function. The most likely CF_BITMAP
//        is a monochrome bitmap from Paintbrush.
#if WINVER >= 0x300
	HANDLE hpl = GetClipboardData(CF_PALETTE);
	HPALETTE idPal=(HPALETTE)NULL;
	HPALETTE oPal=(HPALETTE)NULL;
#endif
	HANDLE han = GetClipboardData(CF_BITMAP);
	unsigned int bmpStride;
	BITMAPFILEHEADER bfh;
	unsigned int obmpbpp;
	unsigned int bmpbpp;
	unsigned int bmisz;
#if WINVER >= 0x300
	HDC cmpDC,screenDC;
#endif
	BITMAPINFO *bmi;
	HGLOBAL hmem;
	DWORD bmsz;
	BITMAP bm;
	int fd;

#if TARGET_MSDOS == 16
	void FAR *p = NULL;
#else
	void *p = NULL;
#endif

	memset(&bm,0,sizeof(bm));
	if (GetObject((HGDIOBJ)han,sizeof(bm),&bm) != sizeof(bm))
		return;
	if (bm.bmBitsPixel == 0 || bm.bmPlanes == 0)
		return;

	bmsz = (DWORD)bm.bmHeight * (DWORD)bm.bmWidthBytes * (DWORD)bm.bmPlanes;
	bmpbpp = bm.bmPlanes * bm.bmBitsPixel;

	if (bmpbpp <= 1) obmpbpp = 1;
	else if (bmpbpp <= 4) obmpbpp = 4;
	else if (bmpbpp <= 8) obmpbpp = 8;
	else obmpbpp = 24;

	bmpStride = bitmap_stride_from_bpp_and_w(obmpbpp,bm.bmWidth);

	if (!bmsz || !bmpStride || !bmpbpp)
		return;

#if WINVER < 0x300
	// Windows 1.x/2.x: monochrome only, everything else is device dependent
	if (bm.bmBitsPixel != 1 || bm.bmPlanes != 1)
		return;
#endif

	bmisz = sizeof(BITMAPINFOHEADER);
	if (bmpbpp <= 8) bmisz += sizeof(RGBQUAD) << bmpbpp;
	bmi = (BITMAPINFO*)malloc(bmisz);
	if (!bmi) return;
	memset(bmi,0,bmisz);
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = bm.bmWidth;
	bmi->bmiHeader.biHeight = bm.bmHeight;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = obmpbpp;
	bmi->bmiHeader.biSizeImage = (DWORD)bm.bmHeight * (DWORD)bmpStride;
	if (bmpbpp <= 8) bmi->bmiHeader.biClrUsed = bmi->bmiHeader.biClrImportant = 1 << bmpbpp;

	memset(&bfh,0,sizeof(bfh));
	bfh.bfType = 0x4D42; // BM
	bfh.bfSize = (DWORD)bmisz + (DWORD)sizeof(bfh) + bmi->bmiHeader.biSizeImage;
	bfh.bfOffBits = (DWORD)sizeof(bfh) + (DWORD)bmisz;

#if WINVER >= 0x300
	screenDC = GetDC((HWND)NULL);
	cmpDC = CreateCompatibleDC(screenDC);
	if (cmpDC) {
		if (bmpbpp <= 8) {
			if (hpl) {
				oPal = SelectPalette(cmpDC,hpl,FALSE);
				RealizePalette(cmpDC);
			}
			else {
				/* No CF_PALETTE (Windows 3.0)? Use the screen palette. */
				idPal = CreateIdentityPalette(bmi->bmiHeader.biClrUsed);
				if (idPal) {
					oPal = SelectPalette(cmpDC,idPal,FALSE);
					RealizePalette(cmpDC);
				}
			}
		}
	}
#endif

	hmem = GlobalAlloc(GHND,bmi->bmiHeader.biSizeImage);
	if (hmem) p = GlobalLock(hmem);

#if TARGET_MSDOS == 16
	// FIXME: open() in real-mode Windows is unable to open and create a file if it doesn't exist??
	{
		int han = 0;
		if (_dos_creat("BITMAP.BMP",0,&han) == 0) _dos_close(han);
	}
#endif
	fd = open("BITMAP.BMP",O_WRONLY|O_TRUNC|O_CREAT|O_BINARY,0644);

#if WINVER >= 0x300
	if (p && cmpDC && fd) {
		write(fd,&bfh,sizeof(bfh));
		GetDIBits(cmpDC,(HBITMAP)han,0,bm.bmHeight,p,bmi,DIB_RGB_COLORS);
		write(fd,bmi,bmisz);
		DumpToFile(fd,p,bmi->bmiHeader.biSizeImage);
	}
#else
	if (p && fd) {
		GetBitmapBits((HBITMAP)han,(LONG)bmi->bmiHeader.biSizeImage,p);
	}
#endif

	if (fd) close(fd);
	if (p) GlobalUnlock(hmem);
	if (hmem) GlobalFree(hmem);

#if WINVER >= 0x300
	if (cmpDC) {
		if (oPal) SelectPalette(cmpDC,oPal,TRUE);
		DeleteObject(cmpDC);
	}
	if (idPal) DeleteObject(idPal);
	ReleaseDC((HWND)NULL,screenDC);
#endif
	free(bmi);
}

static void DumpDIBasBMP(void) {
	HANDLE han = GetClipboardData(CF_DIB);
	if (han) {
		DWORD psz = GlobalSize((HGLOBAL)han);
#if TARGET_MSDOS == 16
		void FAR *p = GlobalLock((HGLOBAL)han);
		BITMAPINFO FAR *bm = (BITMAPINFO FAR *)p;
#else
		void *p = GlobalLock((HGLOBAL)han);
		BITMAPINFO *bm = (BITMAPINFO*)p;
#endif
		if (p) {
			/* everything for a BMP is there, except the BMP file header */
			BITMAPFILEHEADER bfh;
			int fd;

			memset(&bfh,0,sizeof(bfh));
			bfh.bfType = 0x4D42; // BM
			bfh.bfSize = (DWORD)bm->bmiHeader.biSize + (DWORD)sizeof(bfh) + bm->bmiHeader.biSizeImage;
			bfh.bfOffBits = (DWORD)sizeof(bfh) + (DWORD)bm->bmiHeader.biSize;
			if (bm->bmiHeader.biBitCount <= 8) {
				if (bm->bmiHeader.biClrUsed != 0 && bm->bmiHeader.biClrUsed <= 256)
					bfh.bfOffBits += (DWORD)4 * (DWORD)bm->bmiHeader.biClrUsed;
				else
					bfh.bfOffBits += (DWORD)4 << (DWORD)bm->bmiHeader.biBitCount;
			}

#if TARGET_MSDOS == 16
			// FIXME: open() in real-mode Windows is unable to open and create a file if it doesn't exist??
			{
				int han = 0;
				if (_dos_creat("DIB.BMP",0,&han) == 0) _dos_close(han);
			}
#endif
			fd = open("DIB.BMP",O_WRONLY|O_TRUNC|O_CREAT|O_BINARY,0644);
			if (fd >= 0) {
				write(fd,&bfh,sizeof(bfh));
				DumpToFile(fd,p,psz);
				GlobalUnlock((HGLOBAL)han);
				close(fd);
			}
		}
	}
}

static void DoClipboardSave(void) {
	unsigned int sel,cn;

	if (OpenClipboard(hwndMain)) {
		BOOL doBMP = FALSE,doDIB = FALSE;

		cn = SendMessage(cbListHwnd,LB_GETCOUNT,0,0);
		if (cn > MAX_CLIPBOARD_FORMATS) cn = MAX_CLIPBOARD_FORMATS;

		for (sel=0;sel < cn;sel++) {
			if (SendMessage(cbListHwnd,LB_GETSEL,sel,0)) {
				DoClipboardSaveFormat(sel);

				if (clipboard_format[sel].cbFmt == CF_BITMAP)
					doBMP = TRUE;
				if (clipboard_format[sel].cbFmt == CF_DIB)
					doDIB = TRUE;
			}
		}

		if (doBMP && IsClipboardFormatAvailable(CF_BITMAP))
			DumpBITMAPasBMP();
		if (doDIB && IsClipboardFormatAvailable(CF_DIB))
			DumpDIBasBMP();

		CloseClipboard();
	}
}

static void PopulateClipboardFormats(void) {
	char tmp[128];

	SendMessage(cbListHwnd,LB_RESETCONTENT,0,0);

	if (OpenClipboard(hwndMain)) {
		UINT efmt = 0,nfmt,i = 0;

		while (i < MAX_CLIPBOARD_FORMATS) {
			nfmt = EnumClipboardFormats(efmt);
			if (nfmt == 0) break;
			efmt = nfmt;

			clipboard_format[i].cbFmt = efmt;

			MakeCFName(tmp,tmp+sizeof(tmp)-1,efmt,FALSE);

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

		/* NTS: There is no way to know if this failed, because NULL is a valid response
		 *      in the case that there is no viewer after us */
		cbViewNextHwnd = SetClipboardViewer(hwnd);
		cbViewInit = TRUE;

		return 0; /* Success */
	}
	else if (message == WM_DRAWCLIPBOARD) {
		PopulateClipboardFormats();
		if (cbViewNextHwnd) return SendMessage(cbViewNextHwnd,message,wparam,lparam);
		return 0;
	}
	else if (message == WM_CHANGECBCHAIN) {
		if (cbViewInit) {
			/* wparam == HWND of window being removed
			 * lparam == next window after removed window */
			if ((HWND)wparam == cbViewNextHwnd)
				cbViewNextHwnd = (HWND)LOWORD(lparam);
			else
				return SendMessage(cbViewNextHwnd,message,wparam,lparam);
		}

		return 0;
	}
	else if (message == WM_DESTROY) {
		if (cbListHwnd) DestroyWindow(cbListHwnd);

		if (cbViewInit) {
			ChangeClipboardChain(hwnd,cbViewNextHwnd);
			cbViewNextHwnd = NULL;
			cbViewInit = FALSE;
		}

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
		unsigned int w = LOWORD(lparam),h = HIWORD(lparam);
#if WINVER >= 0x200 /* SetWindowPos() did not appear until Windows 2.x */
		SetWindowPos(cbListHwnd,HWND_TOP,0,0,w,h,SWP_NOACTIVATE);
#else
		MoveWindow(cbListHwnd,0,0,w,h,TRUE);
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

	hwndMain = CreateWindow(WndProcClass,"Clipboard dumper",
		WS_OVERLAPPED|WS_SYSMENU|WS_MINIMIZEBOX,
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

