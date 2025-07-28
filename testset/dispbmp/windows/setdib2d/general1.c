
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

#include "libbmp.h"

typedef void (*conv_scanline_func_t)(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes);

static char*			bmpfile = NULL;

static HWND near		hwndMain;
static const char near		WndProcClass[] = "GENERAL1SETDIBITSTODEVICE";
static HINSTANCE near		myInstance;

static BOOL need_palette = FALSE;
static HGLOBAL bmpMemHandle = 0;
static HGLOBAL bmpInfoHandle = 0;
static HPALETTE bmpPalette = 0;
static struct BMPFILEREAD *bfr = NULL;
static unsigned bmpDIBmode = DIB_RGB_COLORS;
static conv_scanline_func_t convert_scanline;

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
			BITMAPINFO FAR *bmi;
			void FAR *bm;

			BeginPaint(hwnd,&ps);

			bmi = (BITMAPINFO FAR*)GlobalLock(bmpInfoHandle);
			bm = GlobalLock(bmpMemHandle);

			if (bmi && bm) {
				SetDIBitsToDevice(ps.hdc,
					0,0, // dX,dY
					bmi->bmiHeader.biWidth,bmi->bmiHeader.biHeight, // cx,cy (rect w/h)
					0,0, // sX,sY
					0, // first scanline in array
					bmi->bmiHeader.biHeight, // numbe of scanlines
					bm, // bitmap bits
					bmi, // bitmap info
					bmpDIBmode);
			}

			if (bm) GlobalUnlock(bmpMemHandle);
			if (bmi) GlobalUnlock(bmpInfoHandle);

			EndPaint(hwnd,&ps);
		}

		return 0; /* Return 0 to signal we processed the message */
	}
	else {
		return DefWindowProc(hwnd,message,wparam,lparam);
	}

	return 0;
}

void convert_scanline_none(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes) {
	(void)bytes;
	(void)src;
	(void)bfr;
}

void convert_scanline_32bpp8(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes) {
	uint32_t *s32 = (uint32_t*)src;

	while (bytes-- > 0u) {
		uint32_t f;

		f  = ((*s32 >> (uint32_t)bfr->red_shift) & 0xFFu) << (uint32_t)16u;
		f += ((*s32 >> (uint32_t)bfr->green_shift) & 0xFFu) << (uint32_t)8u;
		f += ((*s32 >> (uint32_t)bfr->blue_shift) & 0xFFu) << (uint32_t)0u;
		*s32++ = f;
	}
}

void convert_scanline_16bpp555(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes) {
	uint16_t *s16 = (uint16_t*)src;

	while (bytes-- > 0u) {
		uint16_t f;

		f  = ((*s16 >> (uint16_t)bfr->red_shift) & 0x1Fu) << (uint16_t)10u;
		f += ((*s16 >> (uint16_t)bfr->green_shift) & 0x1Fu) << (uint16_t)5u;
		f += ((*s16 >> (uint16_t)bfr->blue_shift) & 0x1Fu) << (uint16_t)0u;
		*s16++ = f;
	}
}

void convert_scanline_16bpp565(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes) {
	uint16_t *s16 = (uint16_t*)src;

	while (bytes-- > 0u) {
		uint16_t f;

		f  = ((*s16 >> (uint16_t)bfr->red_shift) & 0x1Fu) << (uint16_t)10u;
		f += ((*s16 >> (uint16_t)bfr->green_shift) & 0x3Eu) << (uint16_t)4u; // drop the LSB of green
		f += ((*s16 >> (uint16_t)bfr->blue_shift) & 0x1Fu) << (uint16_t)0u;
		*s16++ = f;
	}
}

static void load_bmp_scanline(unsigned char FAR *dbase,unsigned int line,unsigned char *s) {
	unsigned w = (unsigned)bfr->stride;
#if TARGET_MSDOS == 16 || defined(WIN386)
	const unsigned long ofs = ((unsigned long)(bfr->height - 1u - line) * (unsigned long)bfr->stride) + (unsigned long)FP_OFF(dbase);
# if defined(WIN386)
	unsigned ps = FP_SEG(dbase) + ((unsigned)(ofs >> 16ul) << (unsigned)3u); // TODO: Use __AHSHIFT
# else
	unsigned ps = FP_SEG(dbase) + ((unsigned)(ofs >> 16ul) << (unsigned)3u); // TODO: Use __AHSHIFT
# endif
	unsigned po = (unsigned)ofs;

	if (((unsigned long)po+(unsigned long)w) > 0x10000ul) {
		const unsigned rem = (~po + 1u);

		_fmemcpy(MK_FP(ps,po),s,rem);
		po = 0; ps += 1u << 3u; w -= rem; s += rem; // TODO: Use __AHINCR
		_fmemcpy(MK_FP(ps,po),s,w);
	}
	else {
		_fmemcpy(MK_FP(ps,po),s,w);
	}
#else
	memcpy(dbase+(line*bfr->stride),s,w);
#endif
}

int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	WNDCLASS wnd;
	MSG msg;

	bmpfile = lpCmdLine;
	myInstance = hInstance;

	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		wnd.lpfnWndProc = (WNDPROC)MakeProcInstance((FARPROC)WndProc,hInstance);
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
		300,200,
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
			need_palette = TRUE;

		if (!(rcaps & RC_BITMAP64) || !(rcaps & RC_DIBTODEV)) {
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

	/* how much BITMAPINFO do we need? */
	{
		DWORD sz = sizeof(BITMAPINFOHEADER);

		if (bfr->bpp <= 8) sz += sizeof(RGBQUAD) * (1u << bfr->bpp);
		bmpInfoHandle = GlobalAlloc(GMEM_SHARE|GMEM_ZEROINIT,sz);
		if (!bmpInfoHandle) {
			MessageBox((unsigned)NULL,"Unable to allocate bmp info","Err",MB_OK);
			return 1;
		}
	}
	/* set it up */
	{
		BITMAPINFOHEADER FAR *bih = NULL;
		unsigned int i;
		void FAR *p;

		p = GlobalLock(bmpInfoHandle);
		if (!p) {
			MessageBox((unsigned)NULL,"Unable to lock bmp info","Err",MB_OK);
			return 1;
		}

		bmpDIBmode = DIB_RGB_COLORS;

		bih = (BITMAPINFOHEADER FAR*)p;
		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biWidth = bfr->width;
		bih->biHeight = bfr->height;
		bih->biPlanes = 1;
		bih->biBitCount = bfr->bpp;
		bih->biCompression = 0;
		bih->biSizeImage = bfr->stride * bfr->height;
		if (bfr->bpp <= 8) {
			bih->biClrUsed = bfr->colors;
			bih->biClrImportant = bfr->colors;
		}

		if (bfr->bpp == 4 || bfr->bpp == 8) {
			if (need_palette) {
				uint16_t FAR *pal = (uint16_t FAR*)((unsigned char FAR*)p + sizeof(BITMAPINFOHEADER));
				for (i=0;i < (1u << bfr->bpp);i++) pal[i] = i;
				bmpDIBmode = DIB_PAL_COLORS;
			}
			else {
				RGBQUAD FAR *pal = (RGBQUAD FAR*)((unsigned char FAR*)p + sizeof(BITMAPINFOHEADER));
				if (bfr->colors != 0 && bfr->colors <= (1u << bfr->bpp)) _fmemcpy(pal,bfr->palette,sizeof(RGBQUAD) * bfr->colors);
			}
		}

		GlobalUnlock(bmpInfoHandle);
	}
	/* palette */
	if (bmpDIBmode == DIB_PAL_COLORS) {
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

		bmpPalette = CreatePalette(lp);
		if (!bmpPalette) {
			MessageBox((unsigned)NULL,"Unable to createPalette","Err",MB_OK);
			return 1;
		}

		free(lp);
	}

	/* the bitmap itself */
	bmpMemHandle = GlobalAlloc(GMEM_SHARE|GMEM_ZEROINIT,(DWORD)bfr->height * (DWORD)bfr->stride);
	if (!bmpMemHandle) {
		MessageBox((unsigned)NULL,"Unable to alloc bitmap","Err",MB_OK);
		return 1;
	}

	{
		void FAR *p = GlobalLock(bmpMemHandle);
		if (!p) {
			MessageBox((unsigned)NULL,"Unable to lock bitmap","Err",MB_OK);
			return 1;
		}

		/* OK, now read it in! */
		while (read_bmp_line(bfr) == 0) {
			convert_scanline(bfr,bfr->scanline,bfr->width);
			load_bmp_scanline(p,bfr->current_line,bfr->scanline);
		}

		GlobalUnlock(bmpMemHandle);
	}

	/* done reading */
	close_bmp(&bfr);

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

	if (bmpPalette) DeleteObject(bmpPalette);
	bmpPalette = (unsigned)NULL;
	GlobalFree(bmpMemHandle);
	bmpMemHandle = (unsigned)NULL;
	GlobalFree(bmpInfoHandle);
	bmpInfoHandle = (unsigned)NULL;
	return msg.wParam;
}

