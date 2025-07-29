
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <dos.h>
#include <i86.h>

#include <windows.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>

#include "libbmp.h"

#if TARGET_MSDOS == 16
# define MEM_BY_GLOBALALLOC
#endif

typedef void (*conv_scanline_func_t)(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes);

static char*			bmpfile = NULL;

static HWND near		hwndMain;
static const char near		WndProcClass[] = "GENERAL1SETDIBITSTODEVICE";
static const char near		drawFailMsg[] = "Windows failed to draw the image";
static HINSTANCE near		myInstance;

#ifdef MEM_BY_GLOBALALLOC
struct bmpstrip_t {
	HGLOBAL			stripHandle;
	unsigned int		stripHeight;
};
#endif

struct wndstate_t {
	unsigned int		scrollX,scrollY;

#ifdef MEM_BY_GLOBALALLOC
# define MAX_STRIPS		128
	unsigned int		bmpStripHeight;
	unsigned int		bmpStripCount;
	struct bmpstrip_t	bmpStrips[MAX_STRIPS];
#else
	unsigned char*		bmpMem;
#endif

	unsigned int		bmpWidth;
	unsigned int		bmpHeight;
	unsigned int		bmpStride;
	unsigned char		bmpInfoRaw[sizeof(struct winBITMAPV4HEADER) + (256 * sizeof(RGBQUAD))];

	BOOL			need_palette;
	HPALETTE		bmpPalette;
	unsigned		bmpDIBmode;
};

static void wndstate_init(struct wndstate_t *w) {
	memset(w,0,sizeof(*w));
	w->bmpDIBmode = DIB_RGB_COLORS;
}

// TEMPORARY
static struct wndstate_t tndstate;

static inline BITMAPINFOHEADER* bmpInfo(struct wndstate_t *w) {
	return (BITMAPINFOHEADER*)(w->bmpInfoRaw);
}

static struct BMPFILEREAD *bfr = NULL;
static conv_scanline_func_t convert_scanline;

static void CheckScrollBars(struct wndstate_t *w,HWND hwnd,const unsigned int nWidth,const unsigned int nHeight) {
	if (nWidth < w->bmpWidth)
		SetScrollRange(hwnd,SB_HORZ,0,w->bmpWidth - nWidth,TRUE);
	else
		SetScrollRange(hwnd,SB_HORZ,0,0,TRUE);

	if (nHeight < w->bmpHeight)
		SetScrollRange(hwnd,SB_VERT,0,w->bmpHeight - nHeight,TRUE);
	else
		SetScrollRange(hwnd,SB_VERT,0,0,TRUE);
}

static void CommonScrollPosHandling(HWND hwnd,const unsigned int sb,unsigned int *scrollPos,const unsigned int req,const unsigned int nPos) {
# if defined(WIN386)
	short pMin=0,pMax=0;
# else
	int pMin=0,pMax=0;
# endif
	const int cPos = GetScrollPos(hwnd,sb);
	BOOL redraw = FALSE;
	RECT um;

	GetClientRect(hwnd,&um);
	GetScrollRange(hwnd,sb,&pMin,&pMax);

	switch (req) {
		case SB_BOTTOM:
			SetScrollPos(hwnd,sb,pMax,TRUE); redraw = TRUE; break;
		case SB_TOP:
			SetScrollPos(hwnd,sb,pMin,TRUE); redraw = TRUE; break;
		case SB_LINEDOWN:
			SetScrollPos(hwnd,sb,cPos + 32,TRUE); redraw = TRUE; break;
		case SB_LINEUP:
			SetScrollPos(hwnd,sb,cPos - 32,TRUE); redraw = TRUE; break;
		case SB_PAGEDOWN:
			SetScrollPos(hwnd,sb,cPos + (int)um.bottom,TRUE); redraw = TRUE; break;
		case SB_PAGEUP:
			SetScrollPos(hwnd,sb,cPos - (int)um.bottom,TRUE); redraw = TRUE; break;
		case SB_THUMBTRACK:
			SetScrollPos(hwnd,sb,nPos,TRUE); redraw = TRUE; break;
		case SB_THUMBPOSITION:
			SetScrollPos(hwnd,sb,nPos,TRUE); redraw = TRUE; break;
		default:
			break;
	}

	if (redraw) {
		*scrollPos = GetScrollPos(hwnd,sb);
		InvalidateRect(hwnd,NULL,FALSE);
	}
}

LRESULT PASCAL FAR WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DESTROY) {
		PostQuitMessage(0);
		return 0; /* OK */
	}
	else if (message == WM_SETCURSOR) {
		if (LOWORD(lparam) == HTCLIENT) {
			SetCursor(LoadCursor((unsigned)NULL,IDC_ARROW));
			return 1;
		}
		else {
			return DefWindowProc(hwnd,message,wparam,lparam);
		}
	}
	else if (message == WM_SIZE) {
		const unsigned int nWidth = LOWORD(lparam);
		const unsigned int nHeight = HIWORD(lparam);
		CheckScrollBars(&tndstate,hwnd,nWidth,nHeight);
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
	else if (message == WM_HSCROLL) {
		// Microsoft changed how the info is passed between Win16 and Win32!
#if TARGET_MSDOS == 32 && !defined(WIN386)
		const unsigned int req = LOWORD(wparam);
		const unsigned int nPos = HIWORD(wparam);
#else
		const unsigned int req = LOWORD(wparam);
		const unsigned int nPos = LOWORD(lparam);
#endif
		CommonScrollPosHandling(hwnd,SB_HORZ,&tndstate.scrollX,req,nPos);
	}
	else if (message == WM_VSCROLL) {
		// Microsoft changed how the info is passed between Win16 and Win32!
#if TARGET_MSDOS == 32 && !defined(WIN386)
		const unsigned int req = LOWORD(wparam);
		const unsigned int nPos = HIWORD(wparam);
#else
		const unsigned int req = LOWORD(wparam);
		const unsigned int nPos = LOWORD(lparam);
#endif
		CommonScrollPosHandling(hwnd,SB_VERT,&tndstate.scrollY,req,nPos);
	}
	else if (message == WM_PAINT) {
		BOOL drawFail = FALSE;
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			HPALETTE pPalette = (unsigned)NULL;
			PAINTSTRUCT ps;

			BeginPaint(hwnd,&ps);

#if defined(MEM_BY_GLOBALALLOC)
			if (tndstate.bmpPalette) {
				pPalette = SelectPalette(ps.hdc,tndstate.bmpPalette,FALSE);
				if (pPalette) RealizePalette(ps.hdc);
			}

			{
				BITMAPINFOHEADER *bmi = bmpInfo(&tndstate);
				unsigned int strip=tndstate.bmpStripCount-1u,y=0u;
				void FAR *p;

				for (strip=0;strip < tndstate.bmpStripCount;strip++) {
					p = GlobalLock(tndstate.bmpStrips[strip].stripHandle);
					bmi->biHeight = tndstate.bmpStrips[strip].stripHeight;
					bmi->biSizeImage = bmi->biHeight * tndstate.bmpStride;

					if (p) {
						if (SetDIBitsToDevice(ps.hdc,
							-tndstate.scrollX,y-tndstate.scrollY,
							bmi->biWidth,bmi->biHeight,
							0,0,
							0,
							bmi->biHeight,
							p,
							(BITMAPINFO*)bmi,
							tndstate.bmpDIBmode) == 0)
							drawFail = TRUE;
					}


					GlobalUnlock(tndstate.bmpStrips[strip].stripHandle);
					y += bmi->biHeight;
				}

				bmi->biHeight = tndstate.bmpHeight;
				bmi->biSizeImage = bmi->biHeight * tndstate.bmpStride;
			}

			if (pPalette)
				SelectPalette(ps.hdc,pPalette,TRUE);
#else
			if (tndstate.bmpPalette) {
				pPalette = SelectPalette(ps.hdc,tndstate.bmpPalette,FALSE);
				if (pPalette) RealizePalette(ps.hdc);
			}

			if (tndstate.bmpMem) {
				BITMAPINFOHEADER *bmi = bmpInfo(&tndstate);

				if (SetDIBitsToDevice(ps.hdc,
					-tndstate.scrollX,-tndstate.scrollY,
					bmi->biWidth,bmi->biHeight,
					0,0,
					0,
					bmi->biHeight,
					tndstate.bmpMem,
					(BITMAPINFO*)bmi,
					tndstate.bmpDIBmode) == 0)
					drawFail = TRUE;
			}

			if (pPalette)
				SelectPalette(ps.hdc,pPalette,TRUE);
#endif

			if (drawFail)
				TextOut(ps.hdc,0,0,drawFailMsg,sizeof(drawFailMsg)-1);

			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

void convert_scanline_none(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	(void)pixels;
	(void)src;
	(void)bfr;
}

void convert_scanline_32bpp8(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	uint32_t *s32 = (uint32_t*)src;

	while (pixels-- > 0u) {
		uint32_t f;

		f  = ((*s32 >> (uint32_t)bfr->red_shift) & 0xFFu) << (uint32_t)16u;
		f += ((*s32 >> (uint32_t)bfr->green_shift) & 0xFFu) << (uint32_t)8u;
		f += ((*s32 >> (uint32_t)bfr->blue_shift) & 0xFFu) << (uint32_t)0u;
		*s32++ = f;
	}
}

/* NTS: In Windows 3.x. and BITMAPINFOHEADER, 15/16bpp is always 5:5:5 even IF the video driver is using a 5:6:5 mode!
 *      In Windows 95/98, it is possible to BitBlt 5:6:5 if you use BITMAPINFOV4HEADER and BI_BITFIELDS. */
void convert_scanline_16bpp555(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	uint16_t *s16 = (uint16_t*)src;

	while (pixels-- > 0u) {
		uint16_t f;

		f  = ((*s16 >> (uint16_t)bfr->red_shift) & 0x1Fu) << (uint16_t)10u;
		f += ((*s16 >> (uint16_t)bfr->green_shift) & 0x1Fu) << (uint16_t)5u;
		f += ((*s16 >> (uint16_t)bfr->blue_shift) & 0x1Fu) << (uint16_t)0u;
		*s16++ = f;
	}
}

void convert_scanline_16bpp565(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	uint16_t *s16 = (uint16_t*)src;

	while (pixels-- > 0u) {
		uint16_t f;

		f  = ((*s16 >> (uint16_t)bfr->red_shift) & 0x1Fu) << (uint16_t)10u;
		f += ((*s16 >> (uint16_t)bfr->green_shift) & 0x3Eu) << (uint16_t)4u; // drop the LSB of green
		f += ((*s16 >> (uint16_t)bfr->blue_shift) & 0x1Fu) << (uint16_t)0u;
		*s16++ = f;
	}
}

static void load_bmp_scanline(const unsigned int line,const unsigned char *s) {
#if defined(MEM_BY_GLOBALALLOC)
	const unsigned int strip = line / tndstate.bmpStripHeight;
	if (strip < tndstate.bmpStripCount) {
		const unsigned int subline = line % tndstate.bmpStripHeight;
		if (subline < tndstate.bmpStrips[strip].stripHeight) {
			unsigned int sy = tndstate.bmpStrips[strip].stripHeight - 1u - subline;
			void FAR *p = GlobalLock(tndstate.bmpStrips[strip].stripHandle);
			if (p) {
				_fmemcpy((unsigned char FAR*)p + (sy*tndstate.bmpStride),s,tndstate.bmpStride);
				GlobalUnlock(tndstate.bmpStrips[strip].stripHandle);
			}
		}
	}
#else
	const unsigned int cline = bfr->height - 1u - line;
	memcpy(tndstate.bmpMem+(cline*tndstate.bmpStride),s,tndstate.bmpStride);
#endif
}

int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	//TEMPORARY
	wndstate_init(&tndstate);

	probe_dos();
	detect_windows();

	bmpfile = lpCmdLine;
	myInstance = hInstance;

	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
#if TARGET_MSDOS == 16
		wnd.lpfnWndProc = (WNDPROC)MakeProcInstance((FARPROC)WndProc,hInstance);
#else
		wnd.lpfnWndProc = (WNDPROC)WndProc;
#endif
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = (unsigned)NULL;
		wnd.hCursor = (unsigned)NULL;
		wnd.hbrBackground = (unsigned)NULL;
		wnd.lpszMenuName = NULL;
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox((unsigned)NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
	}

	hwndMain = CreateWindow(WndProcClass,bmpfile,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		1024,1024,
		(unsigned)NULL,(unsigned)NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox((unsigned)NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	/* make sure Windows can handle SetDIBitsToDevice() and bitmaps larger than 64KB and check other things */
	{
		int rcaps,szpal;
		HDC hDC;

		hDC = GetDC(hwndMain);
		if (!hDC) {
			MessageBox((unsigned)NULL,"GetDC err","Err",MB_OK);
			return 1;
		}

		rcaps = GetDeviceCaps(hDC,RASTERCAPS);
		szpal = GetDeviceCaps(hDC,SIZEPALETTE);

		ReleaseDC(hwndMain,hDC);

		if ((rcaps & RC_PALETTE) && szpal > 0)
			tndstate.need_palette = TRUE;

		if (!(rcaps & RC_DIBTODEV)) {
			MessageBox((unsigned)NULL,"Windows GDI lacks features we require (64KB bitmap + SetDIBitsToDevice)","Err",MB_OK);
			return 1;
		}
	}

	if (bmpfile == NULL) {
		MessageBox((unsigned)NULL,"No bitmap specified","Err",MB_OK);
		return 1;
	}

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		MessageBox((unsigned)NULL,"Failed to open BMP","Err",MB_OK);
		return 1;
	}
	if (!(bfr->bpp == 1 || bfr->bpp == 2 || bfr->bpp == 4 || bfr->bpp == 8 || bfr->bpp == 15 || bfr->bpp == 16 || bfr->bpp == 24 || bfr->bpp == 32)) {
		MessageBox((unsigned)NULL,"BMP wrong bit depth","Err",MB_OK);
		return 1;
	}
	if (bfr->bpp <= 8 && (bfr->palette == NULL || bfr->colors == 0)) {
		MessageBox((unsigned)NULL,"BMP missing color palette","Err",MB_OK);
		return 1;
	}

	convert_scanline = convert_scanline_none;

	if (bfr->bpp == 32) {
		if (16u != bfr->red_shift || 8u != bfr->green_shift || 0u != bfr->blue_shift ||
			5u != bfr->red_width || 5u != bfr->green_width || 5u != bfr->blue_width) {
			convert_scanline = convert_scanline_32bpp8;
		}
	}
	else if (bfr->bpp == 16 || bfr->bpp == 15) {
		if (10u != bfr->red_shift || 5u != bfr->green_shift || 0u != bfr->blue_shift ||
			5u != bfr->red_width || 5u != bfr->green_width || 5u != bfr->blue_width) {
			if (bfr->red_width == 5 && bfr->green_width == 5 && bfr->blue_width == 5) {
				convert_scanline = convert_scanline_16bpp555;
			}
			else if (bfr->red_width == 5 && bfr->green_width == 6 && bfr->blue_width == 5) {
				convert_scanline = convert_scanline_16bpp565;
			}
		}
	}

	/* set it up */
	{
		BITMAPINFOHEADER *bih = bmpInfo(&tndstate);/*NTS: data area is big enough even for a 256-color paletted file*/
		unsigned int i;

		tndstate.bmpDIBmode = DIB_RGB_COLORS;

		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biWidth = bfr->width;
		bih->biHeight = bfr->height;
		bih->biPlanes = 1;
		bih->biBitCount = bfr->bpp;
		bih->biCompression = 0;
		bih->biSizeImage = tndstate.bmpStride * bfr->height;
		if (bfr->bpp <= 8) {
			bih->biClrUsed = bfr->colors;
			bih->biClrImportant = bfr->colors;
		}

		if (bfr->bpp == 1 || bfr->bpp == 4 || bfr->bpp == 8) {
			if (tndstate.need_palette) {
				uint16_t FAR *pal = (uint16_t FAR*)((unsigned char FAR*)bih + sizeof(BITMAPINFOHEADER));
				for (i=0;i < (1u << bfr->bpp);i++) pal[i] = i;
				tndstate.bmpDIBmode = DIB_PAL_COLORS;
			}
		}
		if (tndstate.bmpDIBmode == DIB_RGB_COLORS) {
			RGBQUAD FAR *pal = (RGBQUAD FAR*)((unsigned char FAR*)bih + sizeof(BITMAPINFOHEADER));
			if (bfr->colors != 0 && bfr->colors <= (1u << bfr->bpp)) _fmemcpy(pal,bfr->palette,sizeof(RGBQUAD) * bfr->colors);
		}
	}
	/* palette */
	if (tndstate.bmpDIBmode == DIB_PAL_COLORS) {
		unsigned int i;
		LOGPALETTE *lp;

		lp = malloc(sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * (1u << bfr->bpp)));
		if (!lp || bfr->colors > (1u << bfr->bpp)) {
			MessageBox((unsigned)NULL,"Unable to alloc palette struct","Err",MB_OK);
			return 1;
		}

		lp->palVersion = 0x300;
		lp->palNumEntries = bfr->colors;
		for (i=0;i < bfr->colors;i++) {
			lp->palPalEntry[i].peRed = bfr->palette[i].rgbRed;
			lp->palPalEntry[i].peGreen = bfr->palette[i].rgbGreen;
			lp->palPalEntry[i].peBlue = bfr->palette[i].rgbBlue;
			lp->palPalEntry[i].peFlags = 0;
		}

		tndstate.bmpPalette = CreatePalette(lp);
		if (!tndstate.bmpPalette) {
			MessageBox((unsigned)NULL,"Unable to createPalette","Err",MB_OK);
			return 1;
		}

		free(lp);
	}

	tndstate.bmpWidth = bfr->width;
	tndstate.bmpHeight = bfr->height;

	/* the bitmap itself */
#ifdef MEM_BY_GLOBALALLOC
	/* Problem: GlobalAlloc does let you alloc large regions of memory, and StretchDIBitsToDevice()
	 *          from them, but there seems to be bugs in certain drivers that crash Windows if you
	 *          try to draw certain bit depths (S3 drivers in DOSBox). These bugs don't happen if
	 *          you limit the bitmap to strips of less than 64KB each. */
	tndstate.bmpStride = bfr->stride;
	tndstate.bmpStripHeight = 0xFFF0u / tndstate.bmpStride;
	tndstate.bmpStripCount = ((bfr->height + tndstate.bmpStripHeight - 1u) / tndstate.bmpStripHeight);
	if (tndstate.bmpStripCount > MAX_STRIPS) {
		MessageBox((unsigned)NULL,"Bitmap too big (tall)","Err",MB_OK);
		return 1;
	}

	{
		unsigned int i,h=bfr->height;
		for (i=0;i < tndstate.bmpStripCount && h >= tndstate.bmpStripHeight;i++) {
			tndstate.bmpStrips[i].stripHeight = tndstate.bmpStripHeight;
			tndstate.bmpStrips[i].stripHandle = GlobalAlloc(GMEM_ZEROINIT,tndstate.bmpStripHeight*tndstate.bmpStride);
			if (!tndstate.bmpStrips[i].stripHandle) {
				MessageBox((unsigned)NULL,"Unable to alloc bitmap","Err",MB_OK);
				return 1;
			}
			h -= tndstate.bmpStripHeight;
		}
		if (i < tndstate.bmpStripCount && h != 0) {
			tndstate.bmpStrips[i].stripHeight = h;
			tndstate.bmpStrips[i].stripHandle = GlobalAlloc(GMEM_ZEROINIT,h*tndstate.bmpStride);
			if (!tndstate.bmpStrips[i].stripHandle) {
				MessageBox((unsigned)NULL,"Unable to alloc bitmap","Err",MB_OK);
				return 1;
			}
		}
	}
#else
	tndstate.bmpStride = bfr->stride;
	tndstate.bmpMem = malloc(bfr->height * tndstate.bmpStride);
	if (!tndstate.bmpMem) {
		MessageBox((unsigned)NULL,"Unable to alloc bitmap","Err",MB_OK);
		return 1;
	}
#endif
	/* OK, now read it in! */
	while (read_bmp_line(bfr) == 0) {
		convert_scanline(bfr,bfr->scanline,bfr->width);
		load_bmp_scanline(bfr->current_line,bfr->scanline);
	}

	/* done reading */
	close_bmp(&bfr);

	{
		RECT um;
		GetClientRect(hwndMain,&um);
		CheckScrollBars(&tndstate,hwndMain,(unsigned)um.right,(unsigned)um.bottom);
	}

	/* force redraw */
	InvalidateRect(hwndMain,(unsigned)NULL,FALSE);
	UpdateWindow(hwndMain);

	while (GetMessage(&msg,(unsigned)NULL,0,0)) {
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
			PeekMessage(&msg,(unsigned)NULL,0,0,PM_REMOVE);
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
#endif

#ifdef MEM_BY_GLOBALALLOC
	{
		unsigned int i;
		for (i=0;i < tndstate.bmpStripCount;i++) {
			if (tndstate.bmpStrips[i].stripHandle) {
				GlobalFree(tndstate.bmpStrips[i].stripHandle);
				tndstate.bmpStrips[i].stripHandle = (unsigned)NULL;
				tndstate.bmpStrips[i].stripHeight = 0;
			}
		}
	}
#else
	free(tndstate.bmpMem);
	tndstate.bmpMem = NULL;
#endif

	if (tndstate.bmpPalette) DeleteObject(tndstate.bmpPalette);
	tndstate.bmpPalette = (unsigned)NULL;

	return msg.wParam;
}

