
/* WARNING: This program uses SetDIBitsToDevice with bitmaps larger than 64KB.
 *          That works GREAT under Windows 3.1 and higher, but this code
 *          will often trigger weird bugs and crashes under Windows 3.0.
 *          Especially Windows 3.0 386 Enhanced mode.
 *
 *          If you want to display BMPs in Windows 3.0, consider using
 *          GENERAL1.C instead which never causes crashes in Windows 3.0,
 *          which it accomplishes by splitting the bitmap in memory into
 *          slices less than 64KB. To put in another way, the bugs this code
 *          triggers always seem to involve drawing bitmaps that span
 *          multiple 64KB segments. Probably because back then, not many
 *          programmers did that.
 *
 *          Noted: If the bitmap has a width that is a power of 2, and you
 *                 are running Windows 3.0 in 386 enhanced mode, this program
 *                 will cause an Unrecoverable Application Error when it
 *                 calls Windows to draw the bitmap. It doesn't appear
 *                 to matter what video mode. 256-color, 16-color, monochrome,
 *                 etc. doesn't matter. Does not occur in real mode or
 *                 286 standard mode. It also appears for 1280-pixel wide
 *                 images? The power of 2 or 1280-pixel UAE only occurs if
 *                 the scroll position is set to show from (0,0). If you
 *                 tick the scroll bars to the middle of the image before
 *                 it draws, the crash is avoided until you scroll up to
 *                 (0,0) again.
 *
 *          Noted: if you load the 1600x1200 8bpp image, it works, and it
 *                 scrolls and displays without issue, however for some
 *                 reason, SetDIBitsToDevice stops drawing if you scroll
 *                 past... erm... roughly the dimensions of the screen?
 *                 1600x1200 on a 640x480 display will stop updating when
 *                 you scroll past about 2/3 of the way in either dimension.
 *                 Cheap clipping or rejection code? */

#include <sys/types.h>
#include <stdlib.h>
#include <stddef.h>
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

#define IDCSM_INFO		0x7700u

#if TARGET_MSDOS == 16
# define MEM_BY_GLOBALALLOC
#endif

#define WM_USER_SIZECHECK	WM_USER+1

typedef void (*conv_scanline_func_t)(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes);

static HWND near		hwndMain;
static const char near		WndProcClass[] = "GENERAL1SETDIBITSTODEVICE";
static HINSTANCE near		myInstance;

#if TARGET_MSDOS == 16
struct windowextra_t {
	WORD			instance_slot;
};
# define MAX_INSTANCES		32

static unsigned int		my_slot = 0;
#endif

struct wndstate_t {
	unsigned int		scrollX,scrollY;

#ifdef MEM_BY_GLOBALALLOC
	HGLOBAL			bmpHandle;
#else
	unsigned char*		bmpMem;
#endif
	unsigned char*		bmpfile;

	unsigned int		bmpWidth;
	unsigned int		bmpHeight;
	unsigned int		bmpStride;
	unsigned char		bmpInfoRaw[sizeof(struct winBITMAPV4HEADER) + (256 * sizeof(RGBQUAD))];

	BOOL			taken;
	BOOL			drawReady;
	BOOL			isLoading;
	BOOL			need_palette;
	BOOL			isMinimized;
	BOOL			scrollEnable;
	HPALETTE		bmpPalette;
	unsigned		bmpDIBmode;
	uint8_t			src_red_shift;
	uint8_t			src_red_width;
	uint8_t			src_green_shift;
	uint8_t			src_green_width;
	uint8_t			src_blue_shift;
	uint8_t			src_blue_width;
	uint8_t			src_alpha_shift;
	uint8_t			src_alpha_width;
	unsigned char		srcBpp;
};

static void wndstate_init(struct wndstate_t FAR *w) {
#if TARGET_MSDOS == 16 || defined(WIN386)
	_fmemset(w,0,sizeof(*w));
#else
	memset(w,0,sizeof(*w));
#endif
	w->bmpDIBmode = DIB_RGB_COLORS;
}

#if TARGET_MSDOS == 16
static HGLOBAL near inst_state_handle = (HGLOBAL)0; // managed by first instance
static struct wndstate_t FAR *inst_state = NULL; // array of MAX_INSTANCES
#else
static struct wndstate_t FAR the_state;
#endif

static inline BITMAPINFOHEADER FAR* bmpInfo(struct wndstate_t FAR *w) {
	return (BITMAPINFOHEADER FAR*)(w->bmpInfoRaw);
}

static BOOL canBitfields = FALSE; /* can do BI_BITFIELDS (Win95/WinNT 4) */
static BOOL can16bpp = FALSE; /* apparently Windows 3.1 can do 16bpp but only if the screen is 16bpp */
static BOOL can32bpp = FALSE; /* Windows 3.1 doesn't like or support 32bpp ARGB??
                                 Either ignores it, misrenders as black (and can CRASH if more than 64KB!), or misrenders as 24bpp? */
static struct BMPFILEREAD *bfr = NULL;
static conv_scanline_func_t convert_scanline;

static BOOL CheckScrollBars(struct wndstate_t FAR *w,HWND hwnd,const unsigned int nWidth,const unsigned int nHeight) {
	BOOL cW,chg=FALSE;

	/* NTS: Reading the scroll range back to detect scrollbars works well in Windows 3.1, but not so well in Windows 95 */

	cW = (nWidth < w->bmpWidth) || (nHeight < w->bmpHeight);

	if (w->scrollEnable != cW) {
		w->scrollEnable = cW;
		if (cW) {
			SetScrollRange(hwnd,SB_HORZ,0,1,TRUE);
			SetScrollRange(hwnd,SB_VERT,0,1,TRUE);
		}
		else {
			SetScrollRange(hwnd,SB_HORZ,0,0,TRUE);
			SetScrollRange(hwnd,SB_VERT,0,0,TRUE);
		}

		chg = TRUE;
	}

	return chg;
}

static void UpdateScrollBars(struct wndstate_t FAR *w,HWND hwnd,const unsigned int cWidth,const unsigned int cHeight) {
	BOOL redraw = FALSE;

	if (w->scrollEnable) {
		if (cWidth < w->bmpWidth) {
			const unsigned int extra = w->bmpWidth - cWidth;
			if (w->scrollX > extra) { w->scrollX = extra; redraw = TRUE; }
			SetScrollRange(hwnd,SB_HORZ,0,extra,TRUE);
		}
		else {
			if (w->scrollX != 0) { w->scrollX = 0; redraw = TRUE; }
			SetScrollRange(hwnd,SB_HORZ,-1,0,TRUE);
		}

		if (cHeight < w->bmpHeight) {
			const unsigned int extra = w->bmpHeight - cHeight;
			if (w->scrollY > extra) { w->scrollY = extra; redraw = TRUE; }
			SetScrollRange(hwnd,SB_VERT,0,extra,TRUE);
		}
		else {
			if (w->scrollY != 0) { w->scrollY = 0; redraw = TRUE; }
			SetScrollRange(hwnd,SB_VERT,-1,0,TRUE);
		}
	}
	else {
		if (w->scrollX != 0 || w->scrollY != 0) {
			w->scrollX = w->scrollY = 0;
			redraw = TRUE;
		}
	}

	if (redraw) InvalidateRect(hwnd,NULL,FALSE);
}

static void CommonScrollPosHandling(HWND hwnd,const unsigned int sb,unsigned int FAR *scrollPos,const unsigned int req,const unsigned int nPos) {
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

static void UpdateTitleBar(HWND hwnd,struct wndstate_t FAR *work_state) {
	(void)work_state;
	(void)hwnd;

	/* If minimized, show only the file name, else, show the full path */
	if (work_state->isMinimized) {
		char *fp = strrchr(work_state->bmpfile,'\\');
		if (fp) fp++;
		else fp = work_state->bmpfile;
		SetWindowText(hwnd,fp);
	}
	else {
		SetWindowText(hwnd,work_state->bmpfile);
	}
}

static void ShowInfo(HWND hwnd,struct wndstate_t FAR *work_state) {
	char *tmp = malloc(4096),*w = tmp,*f = tmp+4095;
	BITMAPINFOHEADER FAR* bmi = bmpInfo(work_state);

	w += snprintf(w,(int)(f-w),
		"Displaying as: %ux%u %ubpp",
		work_state->bmpWidth,
		work_state->bmpHeight,
		bmi->biBitCount);

	if (work_state->bmpPalette)
		w += snprintf(w,(int)(f-w),", using color palette");
	else if (work_state->need_palette)
		w += snprintf(w,(int)(f-w),", need color palette");

	w += snprintf(w,(int)(f-w),
		"\nSource: %ubpp",
		work_state->srcBpp);
	if (work_state->srcBpp > 8) {
		w += snprintf(w,(int)(f-w),
			" R/G/B/A %u:%u:%u:%u at bit pos %u:%u:%u:%u",
			work_state->src_red_width,
			work_state->src_green_width,
			work_state->src_blue_width,
			work_state->src_alpha_width,
			work_state->src_red_shift,
			work_state->src_green_shift,
			work_state->src_blue_shift,
			work_state->src_alpha_shift);
	}

	w += snprintf(w,(int)(f-w),
		"\nCaps: can16bpp=%u can32bpp=%u canBI_BITFIELDS=%u",
		can16bpp?1:0,
		can32bpp?1:0,
		canBitfields?1:0);

	MessageBox(hwnd,tmp,"Info",MB_OK);

	free(tmp);
}

LRESULT PASCAL FAR WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
#if TARGET_MSDOS == 16
	unsigned instance_slot = GetWindowWord(hwnd,offsetof(struct windowextra_t,instance_slot));
	struct wndstate_t FAR *work_state = &inst_state[instance_slot];
#else
	struct wndstate_t FAR *work_state = &the_state;
#endif

	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_SYSCOMMAND) {
		switch (LOWORD(wparam)) {
			case IDCSM_INFO:
				if (!work_state->isLoading)
					ShowInfo(hwnd,work_state);
				break;
			default:
				return DefWindowProc(hwnd,message,wparam,lparam);
		};
	}
	else if (message == WM_CLOSE) {
		if (work_state->isLoading)
			return 0;

		return DefWindowProc(hwnd,message,wparam,lparam);
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
	else if (message == WM_SIZE || message == WM_USER_SIZECHECK) {
		int nWidth,nHeight;
		RECT rwin,radj,rfin;
		BOOL mini;

		/* Don't use the client area given, get the client area as if scroll bars didn't exist
		 * and use that. Using the client area, along with selectively showing them, can cause
		 * an endless loop and crash Windows 3.1 because showing/hiding the scroll bars changes
		 * the client area, which triggers a WM_SIZE, etc. */
		GetWindowRect(hwnd,&rwin);

		memset(&radj,0,sizeof(radj));
		AdjustWindowRect(&radj,GetWindowLong(hwnd,GWL_STYLE),FALSE/*no menu*/); /* extands outward top/left negative bottom/right positive */

		nWidth = (rwin.right - radj.right) - (rwin.left - radj.left);
		nHeight = (rwin.bottom - radj.bottom) - (rwin.top - radj.top);

		CheckScrollBars(work_state,hwnd,nWidth,nHeight);

		GetClientRect(hwnd,&rfin);
		UpdateScrollBars(work_state,hwnd,rfin.right,rfin.bottom);

		if (message == WM_SIZE) {
			mini = wparam == SIZE_MINIMIZED ? TRUE : IsIconic(hwnd);
			if (work_state->isMinimized != mini) {
				work_state->isMinimized = mini;
				UpdateTitleBar(hwnd,work_state);
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
	else if (message == WM_PALETTECHANGED || message == WM_QUERYNEWPALETTE) {
		if (message == WM_PALETTECHANGED && (HWND)wparam == hwnd)
			return 0;

		if (work_state->bmpPalette) {
			HDC hdc = GetDC(hwnd);
			HPALETTE ppal = SelectPalette(hdc,work_state->bmpPalette,FALSE);
			UINT changed = RealizePalette(hdc);
			SelectPalette(hdc,ppal,FALSE);
			ReleaseDC(hwnd,hdc);

			if (changed) InvalidateRect(hwnd,NULL,FALSE);

			if (message == WM_QUERYNEWPALETTE)
				return changed;
		}
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
		CommonScrollPosHandling(hwnd,SB_HORZ,&(work_state->scrollX),req,nPos);
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
		CommonScrollPosHandling(hwnd,SB_VERT,&(work_state->scrollY),req,nPos);
	}
	else if (message == WM_PAINT) {
		RECT um;

		if (GetUpdateRect(hwnd,&um,TRUE)) {
			HPALETTE pPalette = (unsigned)NULL;
			PAINTSTRUCT ps;

			BeginPaint(hwnd,&ps);

			if (work_state->drawReady) {
#if defined(MEM_BY_GLOBALALLOC)
				if (work_state->bmpPalette) {
					pPalette = SelectPalette(ps.hdc,work_state->bmpPalette,FALSE);
					if (pPalette) RealizePalette(ps.hdc);
				}

				if (work_state->bmpHandle) {
					BITMAPINFOHEADER FAR *bmi = bmpInfo(work_state);
					void FAR *p = GlobalLock(work_state->bmpHandle);

					if (p) {
						SetDIBitsToDevice(ps.hdc,
							-work_state->scrollX,-work_state->scrollY,
							bmi->biWidth,bmi->biHeight,
							0,0,
							0,
							bmi->biHeight,
							p,
							(BITMAPINFO*)bmi,
							work_state->bmpDIBmode);

						GlobalUnlock(work_state->bmpHandle);
					}
				}

				if (work_state->bmpPalette)
					SelectPalette(ps.hdc,pPalette,TRUE);
#else
				if (work_state->bmpPalette) {
					pPalette = SelectPalette(ps.hdc,work_state->bmpPalette,FALSE);
					if (pPalette) RealizePalette(ps.hdc);
				}

				if (work_state->bmpMem) {
					BITMAPINFOHEADER FAR *bmi = bmpInfo(work_state);

					SetDIBitsToDevice(ps.hdc,
						-work_state->scrollX,-work_state->scrollY,
						bmi->biWidth,bmi->biHeight,
						0,0,
						0,
						bmi->biHeight,
						work_state->bmpMem,
						(BITMAPINFO*)bmi,
						work_state->bmpDIBmode);
				}

				if (work_state->bmpPalette)
					SelectPalette(ps.hdc,pPalette,TRUE);
#endif
			}

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

void convert_scanline_32to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	unsigned char *d = (unsigned char*)src;
	uint32_t *s32 = (uint32_t*)src;

	while (pixels-- > 0u) {
		const uint32_t sw = *s32++;
		*d++ = ((sw >> (uint32_t)bfr->blue_shift) & 0xFFu);
		*d++ = ((sw >> (uint32_t)bfr->green_shift) & 0xFFu);
		*d++ = ((sw >> (uint32_t)bfr->red_shift) & 0xFFu);
	}
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

		f  = ((*s16 >> (uint16_t)bfr->red_shift) & 0x1Fu) << (uint16_t)11u;
		f += ((*s16 >> (uint16_t)bfr->green_shift) & 0x3Fu) << (uint16_t)5u;
		f += ((*s16 >> (uint16_t)bfr->blue_shift) & 0x1Fu) << (uint16_t)0u;
		*s16++ = f;
	}
}

void convert_scanline_16bpp565_to_555(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	uint16_t *s16 = (uint16_t*)src;

	while (pixels-- > 0u) {
		uint16_t f;

		f  = ((*s16 >> (uint16_t)bfr->red_shift) & 0x1Fu) << (uint16_t)10u;
		f += ((*s16 >> (uint16_t)bfr->green_shift) & 0x3Eu) << (uint16_t)4u; // drop the LSB of green
		f += ((*s16 >> (uint16_t)bfr->blue_shift) & 0x1Fu) << (uint16_t)0u;
		*s16++ = f;
	}
}

void convert_scanline_16_555to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	unsigned char *d = (unsigned char*)src;
	uint16_t *s16 = (uint16_t*)src;

	/* expansion, so, work from the end */
	s16 += pixels - 1u;
	d += (pixels - 1u) * 3u;

	while (pixels-- > 0u) {
		const uint16_t sw = *s16--;
		d[0] = ((sw >> (uint32_t)bfr->blue_shift) & 0x1Fu) << 3u;
		d[1] = ((sw >> (uint32_t)bfr->green_shift) & 0x1Fu) << 3u;
		d[2] = ((sw >> (uint32_t)bfr->red_shift) & 0x1Fu) << 3u;
		d -= 3;
	}
}

void convert_scanline_16_565to24(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int pixels) {
	unsigned char *d = (unsigned char*)src;
	uint16_t *s16 = (uint16_t*)src;

	/* expansion, so, work from the end */
	s16 += pixels - 1u;
	d += (pixels - 1u) * 3u;

	while (pixels-- > 0u) {
		const uint16_t sw = *s16--;
		d[0] = ((sw >> (uint32_t)bfr->blue_shift) & 0x1Fu) << 3u;
		d[1] = ((sw >> (uint32_t)bfr->green_shift) & 0x3Fu) << 2u;
		d[2] = ((sw >> (uint32_t)bfr->red_shift) & 0x1Fu) << 3u;
		d -= 3;
	}
}

#if defined(MEM_BY_GLOBALALLOC)
static void FAR *bmpHandleLocked = NULL;
#endif

static void load_bmp_scanline(struct wndstate_t FAR *work_state,const unsigned int line,const unsigned char *s) {
	const unsigned int cline = bfr->height - 1u - line;
#if defined(MEM_BY_GLOBALALLOC)
	if (bmpHandleLocked) {
		const unsigned char ahshf = Win16_AHSHIFT();
		const unsigned long ofs = ((unsigned long)cline * (unsigned long)work_state->bmpStride) + (unsigned long)FP_OFF(bmpHandleLocked);
		unsigned sv = FP_SEG(bmpHandleLocked) + ((unsigned)(ofs >> 16ul) << (unsigned)ahshf);
		unsigned ov = (unsigned)(ofs & 0xFFFFul);
		unsigned cpy = (unsigned)work_state->bmpStride;
		const unsigned int rem = 0x10000 - ov;

		if (ov != 0 && rem < cpy) {
			/* crosses two 64KB segments */
			_fmemcpy(MK_FP(sv,ov),s,rem);
			sv += 1u << ahshf; ov = 0;
			cpy -= rem; s += rem;
			_fmemcpy(MK_FP(sv,ov),s,cpy);
		}
		else {
			_fmemcpy(MK_FP(sv,ov),s,cpy);
		}
	}
#else
	memcpy(work_state->bmpMem+(cline*work_state->bmpStride),s,work_state->bmpStride);
#endif
}

static void draw_prog_message_pump(void) {
	MSG msg;

	while (PeekMessage(&msg,(HWND)NULL,0,0,PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static void draw_progress(unsigned int p,unsigned int t) {
	HBRUSH oldBrush,newBrush;
	HPEN oldPen,newPen;
	RECT um;
	HDC hdc;

	GetClientRect(hwndMain,&um);
	{
		int w = (int)(um.right - um.left);
		int h = (int)(um.bottom - um.top);
		int bw = w / 4;
		int bh = 32;
		int x = (w - bw) / 2;
		int y = (h - bh) / 2;

		um.top = y;
		um.left = x;
		um.right = x + bw;
		um.bottom = y + bh;
	}

	hdc = GetDC(hwndMain);

	if (t > 0) {
		int fw = (um.right - um.left) - 1;
		int sw = ((long)fw * (long)p) / (long)t;
		if (sw > fw) sw = fw;

		newPen = (HPEN)GetStockObject(BLACK_PEN);
		newBrush = (HBRUSH)GetStockObject(NULL_BRUSH);

		oldPen = SelectObject(hdc,newPen);
		oldBrush = SelectObject(hdc,newBrush);

		Rectangle(hdc,um.left,um.top,um.right,um.bottom);

		newPen = (HPEN)GetStockObject(NULL_PEN);
		newBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
		SelectObject(hdc,newPen);
		SelectObject(hdc,newBrush);

		Rectangle(hdc,um.left+1,um.top+1,um.left+sw+1,um.bottom);

		newPen = (HPEN)GetStockObject(NULL_PEN);
		newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
		SelectObject(hdc,newPen);
		SelectObject(hdc,newBrush);

		Rectangle(hdc,um.left+sw+1,um.top+1,um.right,um.bottom);

		SelectObject(hdc,oldBrush);
		SelectObject(hdc,oldPen);
	}
	else {
		newPen = (HPEN)GetStockObject(NULL_PEN);
		newBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);

		oldPen = SelectObject(hdc,newPen);
		oldBrush = SelectObject(hdc,newBrush);

		Rectangle(hdc,um.left,um.top,um.right+1,um.bottom+1);

		SelectObject(hdc,oldBrush);
		SelectObject(hdc,oldPen);
	}

	ReleaseDC(hwndMain,hdc);

	draw_prog_message_pump();
}

enum {
	CONV_NONE=0,
	CONV_32TO24,
	CONV_16TO24
};

int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	struct wndstate_t FAR *work_state;
	unsigned int conv = CONV_NONE;
	char* bmpfile = NULL;
	WNDCLASS wnd;
	MSG msg;

	probe_dos();
	detect_windows();

	if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) {
		if (windows_version >= 0x350) { /* NTS: 3.95 or higher == Windows 95 or Windows NT 4.0 */
			canBitfields = TRUE; /* we can use BITMAPV4HEADER and BI_BITFIELDS */
			can32bpp = TRUE; /* can use 32bpp ARGB */
			can16bpp = TRUE; /* can use 16bpp 5:5:5 */
		}
	}

	/* TODO:  If we assign bmpfile = lpCmdLine, it remains valid through this code but Windows APIs
	 *        sometimes gets corrupted or gibberish strings i.e. CreateWindow() and SetWindowText(),
	 *        especially in real-mode Windows 3.0.
	 *
	 *        Making a copy with strdup() seems to solve this issue for some unknown reason.
	 *
	 *        Why? */
	bmpfile = strdup(lpCmdLine);

	myInstance = hInstance;

	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
#if TARGET_MSDOS == 16
		wnd.lpfnWndProc = (WNDPROC)MakeProcInstance((FARPROC)WndProc,hInstance);
#else
		wnd.lpfnWndProc = (WNDPROC)WndProc;
#endif
		wnd.cbClsExtra = 0;
#if TARGET_MSDOS == 16
		wnd.cbWndExtra = sizeof(struct windowextra_t);
#else
		wnd.cbWndExtra = 0;
#endif
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

#if TARGET_MSDOS == 16
		inst_state_handle = GlobalAlloc(GMEM_ZEROINIT|GMEM_SHARE,sizeof(struct wndstate_t) * MAX_INSTANCES);
		if (!inst_state_handle) {
			MessageBox((unsigned)NULL,"Unable to allocate state array","Oops!",MB_OK);
			return 1;
		}

		inst_state = (struct wndstate_t FAR*)GlobalLock(inst_state_handle);
		if (!inst_state) {
			MessageBox((unsigned)NULL,"Unable to lock state array","Oops!",MB_OK);
			return 1;
		}

		{
			unsigned int i;
			for (i=0;i < MAX_INSTANCES;i++) wndstate_init(&inst_state[i]);
		}
#endif
	}
	else {
#if TARGET_MSDOS == 32 && defined(WIN386)
		/* Um, no. This kind of multi instance code does not work right with Win386. Crash, crash, crash.
		 * So we'll just not allow multiple instances. Sorry. */
		MessageBox((unsigned)NULL,"Due to stablity issues, this program does not allow multiple Watcom Win386 instances","Oops!",MB_OK);
		return 1;
#endif
#if TARGET_MSDOS == 16
		GetInstanceData(hPrevInstance,(BYTE near*)(&inst_state_handle),sizeof(inst_state_handle));
		if (!inst_state_handle) {
			MessageBox((unsigned)NULL,"Unable to allocate state array","Oops!",MB_OK);
			return 1;
		}

		inst_state = (struct wndstate_t FAR*)GlobalLock(inst_state_handle);
		if (!inst_state) {
			MessageBox((unsigned)NULL,"Unable to lock state array","Oops!",MB_OK);
			return 1;
		}
#endif
	}

	hwndMain = CreateWindow(WndProcClass,NULL,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,
		320,200,
		(unsigned)NULL,(unsigned)NULL,
		hInstance,NULL);
	if (!hwndMain) {
		MessageBox((unsigned)NULL,"Unable to create window","Oops!",MB_OK);
		return 1;
	}

#if TARGET_MSDOS == 16
	{
		unsigned int i=0;

		while (i < MAX_INSTANCES && inst_state[i].taken) i++;
		if (i >= MAX_INSTANCES) {
			MessageBox((unsigned)NULL,"No available slots","Oops!",MB_OK);
			return 1;
		}

		my_slot = i;
		SetWindowWord(hwndMain,offsetof(struct windowextra_t,instance_slot),my_slot);
		work_state = &inst_state[my_slot];
		work_state->taken = TRUE;
	}
#else
	work_state = &the_state;
	work_state->taken = TRUE;
	wndstate_init(work_state);
#endif
	/* first instance already called init on each element */
	work_state->bmpfile = bmpfile;
	work_state->isLoading = TRUE;

	{
		HMENU SysMenu = GetSystemMenu(hwndMain,FALSE);
		AppendMenu(SysMenu,MF_SEPARATOR,0,"");
		AppendMenu(SysMenu,MF_STRING|MF_DISABLED|MF_GRAYED,IDCSM_INFO,"Image and display &info");
		EnableMenuItem(SysMenu,SC_CLOSE,MF_DISABLED|MF_GRAYED);
	}

	/* make sure Windows can handle SetDIBitsToDevice() and bitmaps larger than 64KB and check other things */
	{
		int rcaps,szpal,bpp,planes;
		HDC hDC;

		hDC = GetDC(hwndMain);
		if (!hDC) {
			MessageBox((unsigned)NULL,"GetDC err","Err",MB_OK);
			work_state->taken = FALSE;
			return 1;
		}

		bpp = GetDeviceCaps(hDC,BITSPIXEL);
		rcaps = GetDeviceCaps(hDC,RASTERCAPS);
		szpal = GetDeviceCaps(hDC,SIZEPALETTE);
		planes = GetDeviceCaps(hDC,PLANES);

		ReleaseDC(hwndMain,hDC);

		if ((rcaps & RC_PALETTE) && szpal > 0)
			work_state->need_palette = TRUE;

		if (!(rcaps & RC_DIBTODEV)) {
			MessageBox((unsigned)NULL,"Windows GDI lacks features we require (SetDIBitsToDevice)","Err",MB_OK);
			work_state->taken = FALSE;
			return 1;
		}

		/* Windows 3.1 can do 16bpp 5:5:5 but only if the screen is 5:5:5.
		 * However, Windows 3.1 cannot do 32bpp ARGB even if the framebuffer itself is 32bpp ARGB,
		 * but it can render 24bpp RGB just fine. */
		if (!can16bpp && windows_version < 0x330) { /* Windows 3.1 and below */
			if (planes == 1 && (bpp == 15 || bpp == 16)) { /* display is 16bpp */
				can16bpp = TRUE;
			}
		}
	}

	if (bmpfile == NULL) {
		MessageBox((unsigned)NULL,"No bitmap specified","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		MessageBox((unsigned)NULL,"Failed to open BMP","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}
	if (bfr->width == 0 || bfr->height == 0 || bfr->width > 8192 || bfr->height > 8192) {
		MessageBox((unsigned)NULL,"BMP with no size","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}

	/* set the window size to the bitmap BEFORE showing it */
	{
		RECT um;

		/* start with client area */
		um.top = um.left = 0;
		um.right = bfr->width;
		um.bottom = bfr->height;

		/* ask Windows to adjust the rect to describe the overall window, frame, titlebar and all.
		 * NTS: This adjusts the top/left negative and the bottom/right positive! */
		AdjustWindowRect(&um,GetWindowLong(hwndMain,GWL_STYLE),FALSE/*no menu*/);

		/* do it */
		SetWindowPos(hwndMain,HWND_TOP,0,0,(int)(um.right-um.left),(int)(um.bottom-um.top),
			SWP_NOACTIVATE|SWP_NOMOVE|SWP_NOZORDER|SWP_NOREDRAW);
	}
	/* if our resizing put it off the screen edge, move it up */
	{
		int borderw,borderh;
		BOOL move=FALSE;
		HDC screenDC;
		RECT um;
		int w,h;

		/* Get window rect for the position to prevent the window from appearing off the bottom/right edge of the screen.
		 * Then prevent the top/left from going off screen. */
		screenDC = GetDC((HWND)NULL);
		GetWindowRect(hwndMain,&um);
		w = GetDeviceCaps(screenDC,HORZRES);
		h = GetDeviceCaps(screenDC,VERTRES);
		borderw = GetSystemMetrics(SM_CXFRAME);
		borderh = GetSystemMetrics(SM_CYFRAME);

		if (um.right > w) {
			const int adjust = um.right - w;
			um.right -= adjust;
			um.left -= adjust;
			move = TRUE;
			if (um.left < 0) um.left = 0;
		}
		if (um.bottom > h) {
			const int adjust = um.bottom - h;
			um.bottom -= adjust;
			um.top -= adjust;
			move = TRUE;
			if (um.top < 0) um.top = 0;
		}

		if (move) {
			SetWindowPos(hwndMain,HWND_TOP,um.left,um.top,(int)(um.right-um.left),(int)(um.bottom-um.top),
				SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOREDRAW);
		}

		ReleaseDC((HWND)NULL,screenDC);
	}

	ShowWindow(hwndMain,nCmdShow);
	UpdateWindow(hwndMain);

	work_state->isMinimized = IsIconic(hwndMain);
	UpdateTitleBar(hwndMain,work_state);

	if (!(bfr->bpp == 1 || bfr->bpp == 2 || bfr->bpp == 4 || bfr->bpp == 8 || bfr->bpp == 15 || bfr->bpp == 16 || bfr->bpp == 24 || bfr->bpp == 32)) {
		MessageBox((unsigned)NULL,"BMP wrong bit depth","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}
	if (bfr->bpp <= 8 && (bfr->palette == NULL || bfr->colors == 0)) {
		MessageBox((unsigned)NULL,"BMP missing color palette","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}

	convert_scanline = convert_scanline_none;

	if (bfr->bpp == 32) {
		if (!can32bpp) {
			conv = CONV_32TO24; /* Windows 3.1 cannot render 32bpp, but it can render 24bpp, so convert on the fly */
			convert_scanline = convert_scanline_32to24;
		}
		else if (16u == bfr->red_shift && 8u == bfr->green_shift && 0u == bfr->blue_shift &&
			5u == bfr->red_width && 5u == bfr->green_width && 5u == bfr->blue_width) {
			/* nothing */
		}
		else {
			convert_scanline = convert_scanline_32bpp8;
		}
	}
	else if (bfr->bpp == 16 || bfr->bpp == 15) {
		if (!can16bpp) {
			conv = CONV_16TO24; /* Windows 3.1 cannot render 16bpp unless 16bpp display, convert to 24bpp */
			if (bfr->green_width > 5)
				convert_scanline = convert_scanline_16_565to24;
			else
				convert_scanline = convert_scanline_16_555to24;
		}
		else if (10u == bfr->red_shift && 5u == bfr->green_shift && 0u == bfr->blue_shift &&
			5u == bfr->red_width && 5u == bfr->green_width && 5u == bfr->blue_width) {
			/* nothing */
		}
		else if (canBitfields &&
			11u == bfr->red_shift && 5u == bfr->green_shift && 0u == bfr->blue_shift &&
			5u == bfr->red_width && 6u == bfr->green_width && 5u == bfr->blue_width) {
			/* nothing */
		}
		else if (bfr->green_width > 5u) {
			if (canBitfields)
				convert_scanline = convert_scanline_16bpp565;
			else
				convert_scanline = convert_scanline_16bpp565_to_555;
		}
		else {
			convert_scanline = convert_scanline_16bpp555;
		}
	}

	work_state->bmpWidth = bfr->width;
	work_state->bmpHeight = bfr->height;

	{
		RECT um;
		GetClientRect(hwndMain,&um); // with no scroll bars
		PostMessage(hwndMain,WM_USER_SIZECHECK,0,0); // let the same WM_SIZE logic set the scroll bars
	}

	draw_progress(0,bfr->height);

	if (bfr->bpp == 32 && conv == CONV_32TO24) {
		work_state->bmpStride = bitmap_stride_from_bpp_and_w(24,work_state->bmpWidth);
	}
	else if ((bfr->bpp == 15 || bfr->bpp == 16) && conv == CONV_16TO24) {
		work_state->bmpStride = bitmap_stride_from_bpp_and_w(24,work_state->bmpWidth);
		if (resize_bmp_scanline(bfr,work_state->bmpStride)) {
			MessageBox((unsigned)NULL,"cannot resize bmp stride","Err",MB_OK);
			work_state->taken = FALSE;
			return 1;
		}
	}
	else {
		work_state->bmpStride = bfr->stride;
	}

	/* set it up */
	{
		BITMAPINFOHEADER FAR *bih = bmpInfo(work_state);/*NTS: data area is big enough even for a 256-color paletted file*/
		unsigned int i;

		work_state->srcBpp = bfr->bpp;
		work_state->src_red_shift = bfr->red_shift;
		work_state->src_red_width = bfr->red_width;
		work_state->src_green_shift = bfr->green_shift;
		work_state->src_green_width = bfr->green_width;
		work_state->src_blue_shift = bfr->blue_shift;
		work_state->src_blue_width = bfr->blue_width;
		work_state->src_alpha_shift = bfr->alpha_shift;
		work_state->src_alpha_width = bfr->alpha_width;
		work_state->bmpDIBmode = DIB_RGB_COLORS;

		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biCompression = 0;

		if (bfr->bpp == 16 || bfr->bpp == 15) {
			if (canBitfields && can16bpp && bfr->green_width > 5u) { // 5:6:5
				struct winBITMAPV4HEADER FAR *bi4 = (struct winBITMAPV4HEADER FAR*)bih;

				bih->biSize = sizeof(struct winBITMAPV4HEADER);
				bih->biCompression = 3; // BI_BITFIELDS

				bi4->bV4RedMask = 0x1Fu << 11u;
				bi4->bV4GreenMask = 0x3Fu << 5u;
				bi4->bV4BlueMask = 0x1Fu << 0u;
			}
		}

		bih->biWidth = bfr->width;
		bih->biHeight = bfr->height;
		bih->biPlanes = 1;

		if (bfr->bpp == 32 && conv == CONV_32TO24)
			bih->biBitCount = 24;
		else if ((bfr->bpp == 15 || bfr->bpp == 16) && conv == CONV_16TO24)
			bih->biBitCount = 24;
		else
			bih->biBitCount = bfr->bpp;

		bih->biSizeImage = work_state->bmpStride * bfr->height;
		if (bfr->bpp <= 8) {
			bih->biClrUsed = bfr->colors;
			bih->biClrImportant = bfr->colors;
		}

		if (bfr->bpp == 1 || bfr->bpp == 4 || bfr->bpp == 8) {
			if (work_state->need_palette) {
				uint16_t FAR *pal = (uint16_t FAR*)((unsigned char FAR*)bih + bih->biSize);
				for (i=0;i < (1u << bfr->bpp);i++) pal[i] = i;
				work_state->bmpDIBmode = DIB_PAL_COLORS;
			}
		}
		if (work_state->bmpDIBmode == DIB_RGB_COLORS) {
			RGBQUAD FAR *pal = (RGBQUAD FAR*)((unsigned char FAR*)bih + bih->biSize);
			if (bfr->colors != 0 && bfr->colors <= (1u << bfr->bpp)) _fmemcpy(pal,bfr->palette,sizeof(RGBQUAD) * bfr->colors);
		}
	}
	/* palette */
	if (work_state->bmpDIBmode == DIB_PAL_COLORS) {
		unsigned int i;
		LOGPALETTE *lp;

		lp = malloc(sizeof(LOGPALETTE) + (sizeof(PALETTEENTRY) * (1u << bfr->bpp)));
		if (!lp || bfr->colors > (1u << bfr->bpp)) {
			MessageBox((unsigned)NULL,"Unable to alloc palette struct","Err",MB_OK);
			work_state->taken = FALSE;
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

		work_state->bmpPalette = CreatePalette(lp);
		if (!work_state->bmpPalette) {
			MessageBox((unsigned)NULL,"Unable to createPalette","Err",MB_OK);
			work_state->taken = FALSE;
			return 1;
		}

		free(lp);
	}

	/* the bitmap itself */
#ifdef MEM_BY_GLOBALALLOC
	work_state->bmpHandle = GlobalAlloc(GMEM_ZEROINIT,(DWORD)work_state->bmpHeight*(DWORD)work_state->bmpStride);
	if (!work_state->bmpHandle) {
		MessageBox((unsigned)NULL,"Unable to alloc bitmap","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}
#else
	work_state->bmpMem = malloc(bfr->height * work_state->bmpStride);
	if (!work_state->bmpMem) {
		MessageBox((unsigned)NULL,"Unable to alloc bitmap","Err",MB_OK);
		work_state->taken = FALSE;
		return 1;
	}
#endif

#if defined(MEM_BY_GLOBALALLOC)
	bmpHandleLocked = GlobalLock(work_state->bmpHandle);
#endif

	{
		unsigned int p=0;

		/* OK, now read it in! */
		draw_progress(p,bfr->height);
		while (read_bmp_line(bfr) == 0) {
			convert_scanline(bfr,bfr->scanline,bfr->width);
			load_bmp_scanline(work_state,bfr->current_line,bfr->scanline);
			draw_progress(++p,bfr->height);
		}

		draw_progress(0,0);
	}

#if defined(MEM_BY_GLOBALALLOC)
	bmpHandleLocked = NULL;
	GlobalUnlock(work_state->bmpHandle);
#endif

	/* done reading */
	close_bmp(&bfr);

	work_state->isLoading = FALSE;
	work_state->drawReady = TRUE;

	{
		HMENU SysMenu = GetSystemMenu(hwndMain,FALSE);
		EnableMenuItem(SysMenu,SC_CLOSE,MF_ENABLED);
		EnableMenuItem(SysMenu,IDCSM_INFO,MF_ENABLED);
	}

	/* force redraw */
	InvalidateRect(hwndMain,(unsigned)NULL,FALSE);
	UpdateWindow(hwndMain);

	while (GetMessage(&msg,(unsigned)NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

#ifdef MEM_BY_GLOBALALLOC
	if (work_state->bmpHandle) {
		GlobalFree(work_state->bmpHandle);
		work_state->bmpHandle = (unsigned)NULL;
	}
#else
	free(work_state->bmpMem);
	work_state->bmpMem = NULL;
#endif

	if (work_state->bmpPalette) DeleteObject(work_state->bmpPalette);
	work_state->bmpPalette = (unsigned)NULL;

#if TARGET_MSDOS == 16
	/* let go of slot */
	work_state->taken = FALSE;
	work_state = NULL;
	my_slot = 0;

	/* Win16 only:
	 * If we are the owner (the first instance that registered the window class),
	 * then we must reside in memory until we are the last instance resident.
	 * If we do not do this, then if multiple instances are open and the user closes US
	 * before closing the others, the others will crash (having pulled the code segment
	 * behind the window class out from the other processes). */
	if (!hPrevInstance) {
		MSG pmsg; /* do not overwrite msg.wParam */

		/* NTS: PeekMessage() PM_REMOVE only, do not translate or dispatch.
		 *      Windows 3.1 tolerates it just fine, but Windows 3.0 will crash. */
		while (GetModuleUsage(hInstance) > 1)
			PeekMessage(&pmsg,(unsigned)NULL,0,0,PM_REMOVE);

		/* let go of our copy of the handle */
		/* NTS: For some weird reason, calling GlobalUnlock() before this waiting loop causes ALL instances
		 *      to lose their data. Calling GlobalUnlock() after the loop fixes that issue. Why? */
		GlobalUnlock(inst_state_handle);
		inst_state = NULL;

		/* only the first instance, who allocated the handle, should free it */
		GlobalFree(inst_state_handle);
		inst_state_handle = 0;
	}
	else {
		/* let go of our copy of the handle */
		GlobalUnlock(inst_state_handle);
		inst_state = NULL;
	}
#endif

	return msg.wParam;
}

