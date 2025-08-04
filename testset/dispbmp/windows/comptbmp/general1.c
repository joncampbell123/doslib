
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
#include <commdlg.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>

/* This code crashes under Win386 no matter what I try under every version of Windows I test.
 * Fuck it, we're not supporting Watcom Win386. */
#if TARGET_MSDOS == 32 && defined(WIN386)
# error This code does not support running under Watcom Win386
#endif

#include "resource.h"

#include "libbmp.h"

#ifndef WM_DISPLAYCHANGE
# define WM_DISPLAYCHANGE 0x007E
#endif

#ifndef WM_SETICON
# define WM_SETICON 0x0080
#endif

#ifndef SM_CXSMICON
# define SM_CXSMICON 49
# define SM_CYSMICON 50
#endif

/* Icon types */
#ifndef ICON_BIG
# define ICON_SMALL 0
# define ICON_BIG 1
#endif

#ifndef SPI_GETWORKAREA
# define SPI_GETWORKAREA 0x0030
#endif

#define WM_USER_SIZECHECK	WM_USER+1

typedef void (*conv_scanline_func_t)(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes);

static const char near		WndProcClass[] = "GENERAL1COMPATBITMAP1";
static HINSTANCE near		myInstance;

#define IDCSM_INFO		0x7700u
#define IDCSM_DDBDUMP		0x7701u
#define IDCSM_ABOUT		0x7702u

struct wndstate_t {
	unsigned int		scrollX,scrollY;

	unsigned int		currentBPP;
	unsigned int		currentPlanes;

	unsigned int		bmpWidth;
	unsigned int		bmpHeight;
	unsigned int		bmpStride;
	unsigned char		bmpInfoRaw[sizeof(struct winBITMAPV4HEADER) + (256 * sizeof(RGBQUAD))];

	unsigned char*		bmpfile;

	unsigned char*		bmpInfoIconRaw;

	BOOL			win95;
	BOOL			drawReady;
	BOOL			isLoading;
	BOOL			need_palette;
	BOOL			isMinimized;
	BOOL			scrollEnable;
	BOOL			displayModeChangeReinit;
	BOOL			hasDoneFirstTimeWindowPos;

	BOOL			canBitfields; /* can do BI_BITFIELDS (Win95/WinNT 4) */
	BOOL			can16bpp;     /* apparently Windows 3.1 can do 16bpp but only if the screen is 16bpp */

	/* Windows 3.1 doesn't like or support 32bpp ARGB??
	   Either ignores it, misrenders as black (and can CRASH if more than 64KB!), or misrenders as 24bpp? */
	BOOL			can32bpp;

	HWND			hwndMain;
	HPALETTE		bmpPalette;
	HBITMAP			bmpHandle,bmpOld;
	HBITMAP			bmpIcon,bmpIconOld;
	HICON			bmpIconIcon;
	HBITMAP			bmpIconSmall,bmpIconSmallOld;
	HICON			bmpIconSmallIcon;
	HDC			bmpDC;
	HDC			bmpIconDC;
	HDC			bmpIconSmallDC;
	POINT			windowSizeMax;
	RECT			desktopWorkArea;
	unsigned		bmpDIBmode;
	unsigned		bmpIconDIBmode;
	unsigned short		iconWidth,iconHeight;
	unsigned short		iconSmallWidth,iconSmallHeight;
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
#if TARGET_MSDOS == 16
	_fmemset(w,0,sizeof(*w));
#else
	memset(w,0,sizeof(*w));
#endif
	w->bmpDIBmode = DIB_RGB_COLORS;
}

static struct wndstate_t the_state;

static inline BITMAPINFOHEADER FAR* bmpInfoIcon(struct wndstate_t FAR *w) {
	return (BITMAPINFOHEADER FAR*)(w->bmpInfoIconRaw);
}

static inline BITMAPINFOHEADER FAR* bmpInfo(struct wndstate_t FAR *w) {
	return (BITMAPINFOHEADER FAR*)(w->bmpInfoRaw);
}

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
	const int cPos = GetScrollPos(hwnd,sb);
	int pMin = 0,pMax = 0;
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
		work_state->can16bpp?1:0,
		work_state->can32bpp?1:0,
		work_state->canBitfields?1:0);

	if (work_state->bmpHandle) {
		BITMAP bm;
		memset(&bm,0,sizeof(bm));
#if TARGET_MSDOS == 16
		if (GetObject(work_state->bmpHandle,sizeof(bm),(void FAR*)(&bm)) >= sizeof(bm)) {
#else
		if (GetObject(work_state->bmpHandle,sizeof(bm),(void*)(&bm)) >= sizeof(bm)) {
#endif
			w += snprintf(w,(int)(f-w),
				"\nGDI BITMAP: %ux%u %ubpp %u-plane bytes/line=%u",
				bm.bmWidth,
				bm.bmHeight,
				bm.bmBitsPixel,
				bm.bmPlanes,
				bm.bmWidthBytes);
		}
	}

	w += snprintf(w,(int)(f-w),
		"\nMax size: %ux%u",
		(unsigned)work_state->windowSizeMax.x,
		(unsigned)work_state->windowSizeMax.y);

	w += snprintf(w,(int)(f-w),
		" Desktop work area: %dx%d-%dx%d",
		(int)work_state->desktopWorkArea.left,
		(int)work_state->desktopWorkArea.top,
		(int)work_state->desktopWorkArea.right,
		(int)work_state->desktopWorkArea.bottom);

	w += snprintf(w,(int)(f-w),
		"\nDisplay: %ubpp %uplanes",
		(int)work_state->currentBPP,
		(int)work_state->currentPlanes);

	MessageBox(hwnd,tmp,"Info",MB_OK);

	free(tmp);
}

static void DumpDDB(HWND hwnd,struct wndstate_t FAR *work_state);

BOOL PASCAL AboutDlgProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	/* NTS: Despite MakeProcInstance() this is still valid because the proc instance
	 *      was created right when starting this dialog box from the same instance */
	struct wndstate_t FAR *work_state = &the_state;

	(void)wparam;
	(void)lparam;

	if (message == WM_INITDIALOG) {
		if (!work_state->isMinimized) {
			RECT me,mom;

			GetWindowRect(GetParent(hwnd),&mom);
			GetWindowRect(hwnd,&me);

			/* Windows 3.1 puts this dialog in the upper left corner.
			 * Please center it in the window. */
			SetWindowPos(hwnd,HWND_TOP,
				(((mom.right - mom.left) - (me.right - me.left)) / 2) + mom.left,
				(((mom.bottom - mom.top) - (me.bottom - me.top)) / 2) + mom.top,
				0,0,
				SWP_NOSIZE);
		}

		return TRUE;
	}
	else if (message == WM_COMMAND) {
		if (wparam == IDOK) {
			EndDialog(hwnd,0);
			return TRUE;
		}
	}

	return FALSE;
}

static void ComputeIdealWindowSizeFromImage(RECT *um,HWND hwnd,const unsigned int width,const unsigned int height) {
	/* start with client area */
	um->top = um->left = 0;
	um->bottom = height;
	um->right = width;

	/* ask Windows to adjust the rect to describe the overall window, frame, titlebar and all.
	 * NTS: This adjusts the top/left negative and the bottom/right positive! */
	AdjustWindowRect(um,GetWindowLong(hwnd,GWL_STYLE),FALSE/*no menu*/);

	/* normalize the rect so top/left is zero */
	um->bottom -= um->top;
	um->right -= um->left;
	um->top = um->left = 0;
}

/* For Windows 95, we want to know how much screen we can use without overlapping the taskbar.
 * Note that the user can put the taskbar on any side of the screen they want, therefore even
 * though the most common case is a RECT ot 0,0,screen width,screen height-taskbar, it is also
 * possible to put the taskbar on top and get 0,taskbar,screen width,screen height. Windows
 * returns a RECT for a reason, obviously!
 *
 * Windows 3.1 will not fill in the rectangle at all, because GETWORKAREA did not exist at the time.
 *
 * SystemParametersInfo() did not exist in Windows 3.0, hence the GetProcAddress() call. */
static void queryDesktopWorkArea(struct wndstate_t FAR *work_state,RECT *wa) {
	wa->top = 0;
	wa->left = 0;
	wa->right = GetSystemMetrics(SM_CXSCREEN);
	wa->bottom = GetSystemMetrics(SM_CYSCREEN);

	if (work_state->win95) {
#if TARGET_MSDOS == 16
		HMODULE user = GetModuleHandle("USER");
		if (user) {
			BOOL PASCAL FAR (*SYSPARAMINFO)(UINT,UINT,void FAR*,UINT) = GetProcAddress(user,"SYSTEMPARAMETERSINFO");
			if (SYSPARAMINFO) {
				SYSPARAMINFO(SPI_GETWORKAREA,0,wa,0);
			}
		}
#elif TARGET_MSDOS == 32
		HMODULE user = GetModuleHandle("USER32");
		if (user) {
			BOOL WINAPI FAR (*SYSPARAMINFO)(UINT,UINT,void*,UINT) = GetProcAddress(user,"SystemParametersInfoA");
			if (SYSPARAMINFO) {
				SYSPARAMINFO(SPI_GETWORKAREA,0,wa,0);
			}
		}
#else
		(void)wa;
#endif
	}
}

static void CheckIconSizes(struct wndstate_t FAR *work_state) {
	work_state->iconWidth = GetSystemMetrics(SM_CXICON);
	work_state->iconHeight = GetSystemMetrics(SM_CYICON);

	work_state->iconSmallWidth = GetSystemMetrics(SM_CXSMICON);
	work_state->iconSmallHeight = GetSystemMetrics(SM_CYSMICON);
}

LRESULT PASCAL FAR WndProc(HWND hwnd,UINT message,WPARAM wparam,LPARAM lparam) {
	struct wndstate_t FAR *work_state = &the_state;

	if (message == WM_CREATE) {
		return 0; /* Success */
	}
	else if (message == WM_DISPLAYCHANGE) {
		/* Windows 95: The user or someone else changed the display resolution */
		queryDesktopWorkArea(work_state,&work_state->desktopWorkArea);

		/* Did the display depth change? If so, this program needs to reinitialize itself! */
		if (wparam && wparam != work_state->currentBPP) {
			work_state->displayModeChangeReinit = TRUE;
			work_state->drawReady = FALSE; /* Windows just invalidated our bitmap, don't draw it, it's junk now! */
		}
	}
	else if (message == WM_WININICHANGE) {
		RECT um;

		/* The user might have changed border size in Windows 3.1.
		 * Notice that Windows 95 and later don't let you change border size anymore.
		 * Maybe because 12-year old me thought it was funny to set the Windows 3.1 sizing borders to
		 * absurdly large values so that every window looked like they were ready to play bumper cars,
		 * ha ha. --Jon */
		ComputeIdealWindowSizeFromImage(&um,hwnd,work_state->bmpWidth,work_state->bmpHeight);
		work_state->windowSizeMax.x = um.right;
		work_state->windowSizeMax.y = um.bottom;

		/* Windows 95 also signals this whenever you change which side of the screen the taskbar sits on,
		 * confirmed using Spy++ / Microsoft Visual C++ 4.2 */
		queryDesktopWorkArea(work_state,&work_state->desktopWorkArea);
	}
	else if (message == WM_SYSCOMMAND) {
		switch (LOWORD(wparam)) {
			case IDCSM_INFO:
				if (!work_state->isLoading)
					ShowInfo(hwnd,work_state);
				break;
			case IDCSM_DDBDUMP:
				if (!work_state->isLoading)
					DumpDDB(hwnd,work_state);
				break;
			case IDCSM_ABOUT:
				{
#if TARGET_MSDOS == 16
					/* NTS: MakeProcInstance is unnecessary and pointless for window procs,
					 *      but must be done for dialog box procs. Don't worry, unlike the
					 *      window class proc, the dialog box proc instance will access the
					 *      correct DS segment because it was made from the instance that
					 *      started it. */
					FARPROC p = MakeProcInstance((FARPROC)AboutDlgProc,myInstance);
					DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwnd,p);
					FreeProcInstance(p);
#else
					DialogBox(myInstance,MAKEINTRESOURCE(IDD_ABOUT),hwnd,AboutDlgProc);
#endif
				}
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
	else if (message == WM_GETMINMAXINFO) {
#if TARGET_MSDOS == 16
		MINMAXINFO FAR *mmi = (MINMAXINFO FAR*)lparam;
#else
		MINMAXINFO *mmi = (MINMAXINFO*)lparam;
#endif

		/* don't let the user maximize the window beyond the image size */
		if (mmi->ptMaxSize.x > work_state->windowSizeMax.x)
			mmi->ptMaxSize.x = work_state->windowSizeMax.x;
		if (mmi->ptMaxSize.y > work_state->windowSizeMax.y)
			mmi->ptMaxSize.y = work_state->windowSizeMax.y;

		/* don't let the user resize the window beyond the image size */
		if (mmi->ptMaxTrackSize.x > work_state->windowSizeMax.x)
			mmi->ptMaxTrackSize.x = work_state->windowSizeMax.x;
		if (mmi->ptMaxTrackSize.y > work_state->windowSizeMax.y)
			mmi->ptMaxTrackSize.y = work_state->windowSizeMax.y;

		return 0;
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
				InvalidateRect(hwnd,NULL,TRUE);
			}
		}
	}
	else if (message == WM_ERASEBKGND) {
		RECT um;

		if (work_state->isMinimized) {
			DefWindowProc(hwnd,WM_ICONERASEBKGND,wparam,lparam);
		}
		else if (work_state->isLoading) { /* filling the background is only necessary when loading, the image fills the window now */
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
		}

		return 1; /* Important: Returning 1 signals to Windows that we processed the message. Windows 3.0 gets really screwed up if we don't! */
	}
	else if (message == WM_PALETTECHANGED || message == WM_QUERYNEWPALETTE) {
		if (message == WM_PALETTECHANGED && (HWND)wparam == hwnd)
			return 0;

		if (work_state->bmpPalette) {
			HDC hdc = GetDC(hwnd);
			HPALETTE ppal = SelectPalette(hdc,work_state->bmpPalette,work_state->isMinimized ? TRUE : FALSE);
			UINT changed = RealizePalette(hdc);
			SelectPalette(hdc,ppal,FALSE);
			ReleaseDC(hwnd,hdc);

			InvalidateRect(hwnd,NULL,work_state->isMinimized ? TRUE : FALSE);

			if (message == WM_QUERYNEWPALETTE)
				return changed;
		}
	}
	else if (message == WM_HSCROLL) {
		// Microsoft changed how the info is passed between Win16 and Win32!
#if TARGET_MSDOS == 32
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
#if TARGET_MSDOS == 32
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
				if (work_state->bmpPalette) {
					pPalette = SelectPalette(ps.hdc,work_state->bmpPalette,work_state->isMinimized ? TRUE : FALSE);
					if (pPalette) RealizePalette(ps.hdc);
				}

				if (work_state->isMinimized) {
					// NTS: Windows 95 will never call WM_PAINT when you are minimized,
					//      even IF you make Program Manager your shell instead of Windows Explorer.
					if (work_state->bmpIconDC) {
						int x,y;

						GetClientRect(hwnd,&um);
						x = (um.right - work_state->iconWidth) / 2;
						y = (um.bottom - work_state->iconHeight) / 2;

						BitBlt(ps.hdc,
							x,
							y,
							work_state->iconWidth,
							work_state->iconHeight,
							work_state->bmpIconDC,
							0,0,SRCCOPY);
					}
				}
				else {
					if (work_state->bmpDC) {
						BitBlt(ps.hdc,
							-work_state->scrollX,
							-work_state->scrollY,
							work_state->bmpWidth,
							work_state->bmpHeight,
							work_state->bmpDC,
							0,0,SRCCOPY);
					}
				}

				if (work_state->bmpPalette)
					SelectPalette(ps.hdc,pPalette,TRUE);
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

static unsigned int load_bmp_icony_last = ~0u;
static unsigned int load_bmp_iconsmy_last = ~0u;

static void load_bmp_scanline(struct wndstate_t FAR *work_state,const unsigned int line,const unsigned char *s) {
	const unsigned int cline = work_state->bmpHeight - 1u - line;
#if TARGET_MSDOS == 16
	BITMAPINFO FAR *bm = (BITMAPINFO FAR*)bmpInfo(work_state);
	BITMAPINFO FAR *bmicon = (BITMAPINFO FAR*)bmpInfoIcon(work_state);
#else
	BITMAPINFO *bm = (BITMAPINFO*)bmpInfo(work_state);
	BITMAPINFO *bmicon = (BITMAPINFO*)bmpInfoIcon(work_state);
#endif
	unsigned int icony,iconsmy;

	SetDIBits(work_state->bmpDC,work_state->bmpHandle,
		cline,1,/*start,lines*/
#if TARGET_MSDOS == 16
		(void FAR*)s,
#else
		(void*)s,
#endif
		bm,
		work_state->bmpDIBmode);

	icony = (unsigned int)(((unsigned long)line * (unsigned long)work_state->iconHeight) / (unsigned long)work_state->bmpHeight);
	if (load_bmp_icony_last != icony) {
		load_bmp_icony_last = icony;

		/* 1-pixel height spec needed here to work, or else it does nothing, and also, the inverted Y axis is not needed! */
		bmicon->bmiHeader.biHeight = 1;
		bmicon->bmiHeader.biSizeImage = work_state->bmpStride;

		SetStretchBltMode(work_state->bmpIconDC,STRETCH_DELETESCANS);
		StretchDIBits(work_state->bmpIconDC,
			0,icony,work_state->iconWidth,1,/*dest*/
			0,0,work_state->bmpWidth,1,/*src*/
#if TARGET_MSDOS == 16
			(void FAR*)s,
#else
			(void*)s,
#endif
			bmicon,
			work_state->bmpIconDIBmode,
			SRCCOPY);

		/* put it back */
		bmicon->bmiHeader.biHeight = work_state->bmpHeight;
		bmicon->bmiHeader.biSizeImage = work_state->bmpStride * work_state->bmpHeight;
	}

	iconsmy = (unsigned int)(((unsigned long)line * (unsigned long)work_state->iconSmallHeight) / (unsigned long)work_state->bmpHeight);
	if (load_bmp_iconsmy_last != iconsmy) {
		load_bmp_iconsmy_last = iconsmy;

		/* 1-pixel height spec needed here to work, or else it does nothing, and also, the inverted Y axis is not needed! */
		bmicon->bmiHeader.biHeight = 1;
		bmicon->bmiHeader.biSizeImage = work_state->bmpStride;

		SetStretchBltMode(work_state->bmpIconSmallDC,STRETCH_DELETESCANS);
		StretchDIBits(work_state->bmpIconSmallDC,
			0,iconsmy,work_state->iconSmallWidth,1,/*dest*/
			0,0,work_state->bmpWidth,1,/*src*/
#if TARGET_MSDOS == 16
			(void FAR*)s,
#else
			(void*)s,
#endif
			bmicon,
			DIB_RGB_COLORS,
			SRCCOPY);

		/* put it back */
		bmicon->bmiHeader.biHeight = work_state->bmpHeight;
		bmicon->bmiHeader.biSizeImage = work_state->bmpStride * work_state->bmpHeight;
	}
}

static void draw_prog_message_pump(void) {
	MSG msg;

	while (PeekMessage(&msg,(HWND)NULL,0,0,PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

static void draw_progress(struct wndstate_t FAR *work_state,unsigned int p,unsigned int t) {
	HBRUSH oldBrush,newBrush;
	HPEN oldPen,newPen;
	RECT um;
	HDC hdc;

	GetClientRect(work_state->hwndMain,&um);
	{
		int w = (int)(um.right - um.left);
		int h = (int)(um.bottom - um.top);
		int bw = work_state->isMinimized ? w : (w / 4);
		int bh = work_state->isMinimized ? (h / 2) : 32;
		int x = (w - bw) / 2;
		int y = (h - bh) / 2;

		um.top = y;
		um.left = x;
		um.right = x + bw;
		um.bottom = y + bh;
	}

	hdc = GetDC(work_state->hwndMain);

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
		InvalidateRect(work_state->hwndMain,&um,TRUE);
	}

	ReleaseDC(work_state->hwndMain,hdc);

	draw_prog_message_pump();
}

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

static void DumpDDB(HWND hwnd,struct wndstate_t FAR *work_state) {
	DWORD sz,copied=0;
	BITMAP bm;

	if (!work_state->bmpHandle || !work_state->bmpDC)
		return;

	memset(&bm,0,sizeof(bm));
	if (GetObject(work_state->bmpHandle,sizeof(bm),&bm) < sizeof(bm))
		return;

	sz = (DWORD)bm.bmHeight * (DWORD)bm.bmWidthBytes * (DWORD)bm.bmPlanes;
	if (!sz)
		return;

	work_state->isLoading = TRUE;
	draw_progress(work_state,0,100);

	{
#if TARGET_MSDOS == 16
		unsigned char FAR *p;
#else
		unsigned char *p;
#endif
		HGLOBAL gl;
		int fd;

		gl = GlobalAlloc(GMEM_MOVEABLE,sz);
		if (gl) {
			p = GlobalLock(gl);
			if (p) {
				draw_progress(work_state,10,100);

				copied = (DWORD)GetBitmapBits(work_state->bmpHandle,sz,p);

				draw_progress(work_state,20,100);

				fd = open("ddbdump.bin",O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
				if (fd >= 0) {
#if TARGET_MSDOS == 16
					/* NTS: Watcom C does not help us here and does not provide FAR pointer math when adding to p,
					 *      so we have to FAR pointer math ourself, manually. */
					const unsigned int AHINCR = Win16_AHINCR();
					unsigned int sv = FP_SEG(p),ov = FP_OFF(p);
					const unsigned int blksz = 0x4000u;
					DWORD rem = copied;
					unsigned int todo;

					draw_progress(work_state,30,100);

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
					write(fd,p,copied);
#endif
					close(fd);
				}

				GlobalUnlock(gl);
			}
			GlobalFree(gl);
		}
	}

	draw_progress(work_state,80,100);

	{
		FILE *fp = fopen("ddbdump.txt","w");
		if (fp) {
			fprintf(fp,"bmType=0x%x\n",bm.bmType);
			fprintf(fp,"bmWidth=%d\n",bm.bmWidth);
			fprintf(fp,"bmHeight=%d\n",bm.bmHeight);
			fprintf(fp,"bmWidthBytes=%d\n",bm.bmWidthBytes);
			fprintf(fp,"bmPlanes=%d\n",bm.bmPlanes);
			fprintf(fp,"bmBitsPixel=%u\n",bm.bmBitsPixel);
			fprintf(fp,"computedSize=%lu\n",(unsigned long)sz);
			fprintf(fp,"copiedSize=%lu\n",(unsigned long)copied);
			fclose(fp);
		}
	}

	{
		int colors = GetDeviceCaps(work_state->bmpDC,SIZEPALETTE);
		if (colors == 0) colors = GetDeviceCaps(work_state->bmpDC,NUMCOLORS);
		if (colors > 0 && bm.bmBitsPixel <= 8) {
			BITMAPINFOHEADER *bmh = malloc(sizeof(BITMAPINFOHEADER) + (colors * sizeof(RGBQUAD)));
			if (bmh) {
				HPALETTE oPal=(HPALETTE)NULL,iPal=(HPALETTE)NULL;
				HDC screenDC;

				screenDC = GetDC((HWND)NULL);

				iPal = CreateIdentityPalette(colors);
				if (iPal) {
					oPal = SelectPalette(screenDC,iPal,FALSE);
					RealizePalette(screenDC);
				}

				memset(bmh,0,sizeof(BITMAPINFOHEADER) + (colors * sizeof(RGBQUAD)));
				bmh->biSize = sizeof(BITMAPINFOHEADER);
				bmh->biWidth = bm.bmWidth;
				bmh->biHeight = bm.bmHeight;
				bmh->biPlanes = 1;
				bmh->biBitCount = bm.bmPlanes * bm.bmBitsPixel; /* as if GetDIBits would ever return planar EGA/VGA data */
				bmh->biClrUsed = colors;
				bmh->biClrImportant = colors;

				// GetDIBits() will fill in the palette. We're asking it to return the palette, not bitmap bits,
				// because we only care about the palette.
#if TARGET_MSDOS == 16
				GetDIBits(screenDC,work_state->bmpHandle,0,0,NULL,(BITMAPINFO FAR*)bmh,DIB_RGB_COLORS);
#else
				GetDIBits(screenDC,work_state->bmpHandle,0,0,NULL,(BITMAPINFO*)bmh,DIB_RGB_COLORS);
#endif

				if (oPal)
					SelectPalette(screenDC,oPal,FALSE);
				if (iPal)
					DeleteObject(iPal);

				ReleaseDC((HWND)NULL,screenDC);

				{
					int fd = open("ddbdump.pal",O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,0644);
					if (fd >= 0) {
						write(fd,(unsigned char*)bmh + bmh->biSize,colors * sizeof(RGBQUAD));
						close(fd);
					}
				}

				free(bmh);
			}
		}
	}

	draw_progress(work_state,100,100);
	work_state->isLoading = FALSE;
	InvalidateRect(hwnd,NULL,TRUE);
}

enum {
	CONV_NONE=0,
	CONV_32TO24,
	CONV_16TO24
};

static void cleanup_bmpinfoiconraw(struct wndstate_t *work_state) {
	if (work_state->bmpInfoIconRaw) {
		free(work_state->bmpInfoIconRaw);
		work_state->bmpInfoIconRaw = NULL;
	}
}

static void cleanup_bmpiconsmallicon(struct wndstate_t FAR *work_state) {
	if (work_state->bmpIconSmallIcon) DestroyIcon(work_state->bmpIconSmallIcon);
	work_state->bmpIconSmallIcon = (unsigned)NULL;
}

static void cleanup_bmpiconsmall(struct wndstate_t FAR *work_state) {
	if (work_state->bmpIconSmall && work_state->bmpIconSmallOld) SelectObject(work_state->bmpIconSmallDC,work_state->bmpIconSmallOld);
	if (work_state->bmpIconSmall) DeleteObject(work_state->bmpIconSmall);
	work_state->bmpIconSmall = (unsigned)NULL;
	if (work_state->bmpIconSmallDC) DeleteDC(work_state->bmpIconSmallDC);
	work_state->bmpIconSmallDC = (unsigned)NULL;
}

static void cleanup_bmpiconicon(struct wndstate_t FAR *work_state) {
	if (work_state->bmpIconIcon) DestroyIcon(work_state->bmpIconIcon);
	work_state->bmpIconIcon = (unsigned)NULL;
}

static void cleanup_bmpicon(struct wndstate_t FAR *work_state) {
	if (work_state->bmpIcon && work_state->bmpIconOld) SelectObject(work_state->bmpIconDC,work_state->bmpIconOld);
	if (work_state->bmpIcon) DeleteObject(work_state->bmpIcon);
	work_state->bmpIcon = (unsigned)NULL;
	if (work_state->bmpIconDC) DeleteDC(work_state->bmpIconDC);
	work_state->bmpIconDC = (unsigned)NULL;
}

static void cleanup_bmp(struct wndstate_t *work_state) {
	if (work_state->bmpHandle && work_state->bmpOld) SelectObject(work_state->bmpDC,work_state->bmpOld);
	if (work_state->bmpHandle) DeleteObject(work_state->bmpHandle);
	work_state->bmpHandle = (unsigned)NULL;
	if (work_state->bmpDC) DeleteDC(work_state->bmpDC);
	work_state->bmpDC = (unsigned)NULL;
}

static void cleanup_bmppalette(struct wndstate_t *work_state) {
	if (work_state->bmpPalette) DeleteObject(work_state->bmpPalette);
	work_state->bmpPalette = (unsigned)NULL;
}

static HICON bitmap2icon(HBITMAP hbmp) {
	HICON ico = (HICON)NULL;
	unsigned long andmsz,xormsz;
#if TARGET_MSDOS == 16
	void FAR *andb,FAR *xorb;
#else
	void *andb,*xorb;
#endif
	BITMAP bm;

	if (GetObject(hbmp,sizeof(bm),&bm)) {
		andmsz = (unsigned long)(((bm.bmWidth + 15u) & (~15u)) >> 3u) * (unsigned long)bm.bmHeight;
		xormsz = (unsigned long)bm.bmWidthBytes * (unsigned long)bm.bmHeight;
		if (andmsz < 0xFFF0ul && xormsz < 0xFFF0ul) {
			andb = malloc((unsigned)andmsz);
			xorb = malloc((unsigned)xormsz);

			memset(andb,0x00,(unsigned)andmsz); /* opaque */
			memset(xorb,0x00,(unsigned)xormsz); /* all black */

			GetBitmapBits(hbmp,xormsz,xorb);

			ico = CreateIcon(myInstance,bm.bmWidth,bm.bmHeight,bm.bmPlanes,bm.bmBitsPixel,andb,xorb);

			free(xorb);
			free(andb);
		}
	}

	return ico;
}

/* adjust "um" according to current window position */
static void AddWindowPosToRect(RECT *um,HWND hwnd) {
	RECT curr;

	memset(&curr,0,sizeof(curr));
	GetWindowRect(hwnd,&curr);

	um->top += curr.top;
	um->left += curr.left;
	um->right += curr.left;
	um->bottom += curr.top;
}

static void ClipWindowToWorkArea(struct wndstate_t *work_state,RECT *um) {
	if (um->right > work_state->desktopWorkArea.right) {
		const int adjust = um->right - work_state->desktopWorkArea.right;
		um->right -= adjust;
		um->left -= adjust;
		if (um->left < work_state->desktopWorkArea.left) um->left = work_state->desktopWorkArea.left;
	}
	else if (um->left < work_state->desktopWorkArea.left) {
		const int adjust = work_state->desktopWorkArea.left - um->left;
		um->right += adjust;
		um->left += adjust;
		if (um->right > work_state->desktopWorkArea.right) um->right = work_state->desktopWorkArea.right;
	}

	if (um->bottom > work_state->desktopWorkArea.bottom) {
		const int adjust = um->bottom - work_state->desktopWorkArea.bottom;
		um->bottom -= adjust;
		um->top -= adjust;
		if (um->top < work_state->desktopWorkArea.top) um->top = work_state->desktopWorkArea.top;
	}
	else if (um->top < work_state->desktopWorkArea.top) {
		const int adjust = work_state->desktopWorkArea.top - um->top;
		um->bottom += adjust;
		um->top += adjust;
		if (um->bottom > work_state->desktopWorkArea.bottom) um->bottom = work_state->desktopWorkArea.bottom;
	}
}

static char *CommDlgGetOpenFileName(void) {
	/* TODO:  If we SetErrorMode() to try to turn off all error dialogs in Windows 3.0, and then
	 *        LoadLibrary(), it shows a dialog prompting the user to install that DLL. Why?
	 *        The whole purpose of the call is to shut that dialog up!
	 *
	 *        Well in any case we'll just not try to LOADLIBRARY COMMDLG if below Windows 3.1 */

	/* FIXME: Why does Windows 95 GetOpenFileName() refuse to work for our Win16 builds after a fresh
	 *        boot, and then mysteriously work every time once any Win32 program that uses COMMDLG32.DLL
	 *        loads?
	 *
	 *        Running any Win32 program that depends on COMMDLG32.DLL mysteriously makes it work.
	 *        Our Win32 builds. NOTEPAD.EXE. You don't even have to open a file in that application,
	 *        it just magically makes it work. Why?
	 *
	 *        Whatever the problem is, it seems they fixed it somewhere between Windows 95 and
	 *        Windows 98 because this problem never occurs under Windows 98. */

	BOOL (WINAPI *GETOPENFILENAMEPROC)(OPENFILENAME FAR *) = NULL;
	HMODULE commdlg_dll;
	UINT oldMode;

	oldMode = SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOOPENFILEERRORBOX);
#if TARGET_MSDOS == 16
	commdlg_dll = LoadLibrary("COMMDLG");
	if (commdlg_dll) GETOPENFILENAMEPROC = GetProcAddress(commdlg_dll,"GETOPENFILENAME");
#else
	commdlg_dll = LoadLibrary("COMDLG32.DLL");
	if (commdlg_dll) GETOPENFILENAMEPROC = GetProcAddress(commdlg_dll,"GetOpenFileNameA");
#endif
	SetErrorMode(oldMode);

	if (commdlg_dll && GETOPENFILENAMEPROC) {
		OPENFILENAME ofn;
		char *newname = malloc(PATH_MAX);
		char *rt = NULL;

		if (!newname) {
			FreeLibrary(commdlg_dll);
			return NULL;
		}

		memset(newname,0,PATH_MAX);
		memset(&ofn,0,sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter =
			"Bitmap files\x00" "*.bmp\x00"
			"All files\x00" "*.*\x00"
			"\x00\x00";
		ofn.lpstrFile = newname;
		ofn.nMaxFile = PATH_MAX - 1;
		ofn.nFilterIndex = 1;
		ofn.lpstrTitle = "Pick a BMP file to display";
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;

		if (!GETOPENFILENAMEPROC(&ofn)) {
			FreeLibrary(commdlg_dll);
			free(newname);
			return NULL;
		}

		FreeLibrary(commdlg_dll);
		rt = strdup(newname);
		free(newname);
		return rt;
	}

	if (commdlg_dll)
		FreeLibrary(commdlg_dll);

	return NULL;
}

enum {
	APPLOOP_RESTART = -1
};

static int AppLoop(struct wndstate_t *work_state,int nCmdShow) {
	HPALETTE oldIconPal = (HPALETTE)0;
	HPALETTE oldPal = (HPALETTE)0;
	unsigned int conv = CONV_NONE;
	int retv = 0;
	BOOL gmRet;
	MSG msg;

	work_state->isLoading = TRUE;
	InvalidateRect(work_state->hwndMain,(unsigned)NULL,TRUE);

	{
		HMENU SysMenu = GetSystemMenu(work_state->hwndMain,FALSE);
		EnableMenuItem(SysMenu,SC_CLOSE,MF_DISABLED|MF_GRAYED);
		EnableMenuItem(SysMenu,IDCSM_INFO,MF_DISABLED|MF_GRAYED);
		EnableMenuItem(SysMenu,IDCSM_DDBDUMP,MF_DISABLED|MF_GRAYED);
	}

	/* make sure Windows can handle SetDIBitsToDevice() and bitmaps larger than 64KB and check other things */
	{
		int rcaps;
		HDC hDC;

		hDC = GetDC(work_state->hwndMain);
		rcaps = GetDeviceCaps(hDC,RASTERCAPS);
		ReleaseDC(work_state->hwndMain,hDC);

		if (!(rcaps & RC_DIBTODEV)) {
			MessageBox((unsigned)NULL,"Windows GDI lacks features we require (SetDIBitsToDevice)","Err",MB_OK);
			return 1;
		}
	}

	{
		int rcaps,szpal,bpp,planes;
		HDC hDC;

		hDC = GetDC(work_state->hwndMain);
		bpp = GetDeviceCaps(hDC,BITSPIXEL);
		rcaps = GetDeviceCaps(hDC,RASTERCAPS);
		szpal = GetDeviceCaps(hDC,SIZEPALETTE);
		planes = GetDeviceCaps(hDC,PLANES);
		ReleaseDC(work_state->hwndMain,hDC);

		if ((rcaps & RC_PALETTE) && szpal > 0)
			work_state->need_palette = TRUE;

		/* Windows 3.1 can do 16bpp 5:5:5 but only if the screen is 5:5:5.
		 * However, Windows 3.1 cannot do 32bpp ARGB even if the framebuffer itself is 32bpp ARGB,
		 * but it can render 24bpp RGB just fine. */
		if (!work_state->can16bpp && !work_state->win95) { /* Windows 3.1 and below */
			if (planes == 1 && (bpp == 15 || bpp == 16)) { /* display is 16bpp */
				work_state->can16bpp = TRUE;
			}
		}

		/* keep track in case of Windows 95 display mode changes */
		work_state->currentBPP = bpp;
		work_state->currentPlanes = planes;
	}

	bfr = open_bmp(work_state->bmpfile);
	if (bfr == NULL) {
		MessageBox((unsigned)NULL,"Failed to open BMP","Err",MB_OK);
		return 1;
	}
	if (bfr->width == 0 || bfr->height == 0 || bfr->width > 8192 || bfr->height > 8192) {
		MessageBox((unsigned)NULL,"BMP with no size","Err",MB_OK);
		return 1;
	}

	work_state->bmpWidth = bfr->width;
	work_state->bmpHeight = bfr->height;

	/* set the window size to the bitmap BEFORE showing it. */
	/* if our resizing put it off the screen edge, move it up. */
	/* keep the window within the work area. */
	/* But only do this ONCE at startup. */
	if (!work_state->hasDoneFirstTimeWindowPos) {
		RECT um;

		ComputeIdealWindowSizeFromImage(&um,work_state->hwndMain,work_state->bmpWidth,work_state->bmpHeight);
		work_state->windowSizeMax.x = um.right;
		work_state->windowSizeMax.y = um.bottom;
		AddWindowPosToRect(&um,work_state->hwndMain);
		ClipWindowToWorkArea(work_state,&um);

		/* do it */
		SetWindowPos(work_state->hwndMain,HWND_TOP,um.left,um.top,(int)(um.right-um.left),(int)(um.bottom-um.top),
				SWP_NOACTIVATE|SWP_NOZORDER|SWP_NOREDRAW);

		work_state->hasDoneFirstTimeWindowPos = TRUE;
	}

	ShowWindow(work_state->hwndMain,nCmdShow);
	UpdateWindow(work_state->hwndMain);

	work_state->isMinimized = IsIconic(work_state->hwndMain);
	UpdateTitleBar(work_state->hwndMain,work_state);

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
		if (!work_state->can32bpp) {
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
		if (!work_state->can16bpp) {
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
		else if (work_state->canBitfields &&
				11u == bfr->red_shift && 5u == bfr->green_shift && 0u == bfr->blue_shift &&
				5u == bfr->red_width && 6u == bfr->green_width && 5u == bfr->blue_width) {
			/* nothing */
		}
		else if (bfr->green_width > 5u) {
			if (work_state->canBitfields)
				convert_scanline = convert_scanline_16bpp565;
			else
				convert_scanline = convert_scanline_16bpp565_to_555;
		}
		else {
			convert_scanline = convert_scanline_16bpp555;
		}
	}

	{
		RECT um;
		GetClientRect(work_state->hwndMain,&um); // with no scroll bars
		PostMessage(work_state->hwndMain,WM_USER_SIZECHECK,0,0); // let the same WM_SIZE logic set the scroll bars
	}

	draw_progress(work_state,0,bfr->height);

	if (bfr->bpp == 32 && conv == CONV_32TO24) {
		work_state->bmpStride = bitmap_stride_from_bpp_and_w(24,work_state->bmpWidth);
	}
	else if ((bfr->bpp == 15 || bfr->bpp == 16) && conv == CONV_16TO24) {
		work_state->bmpStride = bitmap_stride_from_bpp_and_w(24,work_state->bmpWidth);
		if (resize_bmp_scanline(bfr,work_state->bmpStride)) {
			MessageBox((unsigned)NULL,"cannot resize bmp stride","Err",MB_OK);
			return 1;
		}
	}
	else {
		work_state->bmpStride = bfr->stride;
	}

	/* allocate temporary buffer */
	work_state->bmpInfoIconRaw = malloc(sizeof(struct winBITMAPV4HEADER) + (256 * sizeof(RGBQUAD)));

	/* set it up */
	{
		BITMAPINFOHEADER FAR *bih = bmpInfo(work_state);/*NTS: data area is big enough even for a 256-color paletted file*/
		BITMAPINFOHEADER FAR *bihicon = bmpInfoIcon(work_state);/*NTS: data area is big enough even for a 256-color paletted file*/
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
		work_state->bmpIconDIBmode = DIB_RGB_COLORS;

		bih->biSize = sizeof(BITMAPINFOHEADER);
		bih->biCompression = 0;

		if (bfr->bpp == 16 || bfr->bpp == 15) {
			if (work_state->canBitfields && work_state->can16bpp && bfr->green_width > 5u) { // 5:6:5
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

		/* Icon is the same.
		 *
		 * Make sure to copy biSize, not just BITMAPINFOHEADER.
		 * If we do not, the icon will render as garbage in the Windows 95 taskbar
		 * if the BMP file we loaded is using the BI_BITFIELDS format. */
#if TARGET_MSDOS == 16
		_fmemcpy(bihicon,bih,bih->biSize);
#else
		memcpy(bihicon,bih,bih->biSize);
#endif

		if (bfr->bpp == 1 || bfr->bpp == 4 || bfr->bpp == 8) {
			if (work_state->need_palette) {
				uint16_t FAR *pal = (uint16_t FAR*)((unsigned char FAR*)bih + bih->biSize);
				for (i=0;i < (1u << bfr->bpp);i++) pal[i] = i;
				work_state->bmpDIBmode = DIB_PAL_COLORS;
			}
			if (work_state->need_palette && !work_state->win95) {
				uint16_t FAR *pal = (uint16_t FAR*)((unsigned char FAR*)bihicon + bih->biSize);
				for (i=0;i < (1u << bfr->bpp);i++) pal[i] = i;
				work_state->bmpIconDIBmode = DIB_PAL_COLORS;
			}
		}
		if (work_state->bmpDIBmode == DIB_RGB_COLORS) {
			RGBQUAD FAR *pal = (RGBQUAD FAR*)((unsigned char FAR*)bih + bih->biSize);
			if (bfr->colors != 0 && bfr->colors <= (1u << bfr->bpp)) _fmemcpy(pal,bfr->palette,sizeof(RGBQUAD) * bfr->colors);
		}
		if (work_state->bmpIconDIBmode == DIB_RGB_COLORS) {
			RGBQUAD FAR *pal = (RGBQUAD FAR*)((unsigned char FAR*)bihicon + bihicon->biSize);
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
			return 1;
		}

		free(lp);
	}

	{
		HDC hdc = GetDC(work_state->hwndMain);

		work_state->bmpDC = CreateCompatibleDC(hdc);
		if (!work_state->bmpDC) {
			MessageBox((unsigned)NULL,"Unable to get compatible DC","Err",MB_OK);
			return 1;
		}

		if (work_state->bmpPalette) {
			oldPal = SelectPalette(work_state->bmpDC,work_state->bmpPalette,TRUE);
			RealizePalette(work_state->bmpDC);
		}

		/* NTS: You could call CreateBitmap to set whatever you want, but if you want to work with the GDI
		 *      regarding bitmaps, you have to match the screen, or else nothing displays and it might also
		 *      cause a crash. At least in Windows 3.1 */
		work_state->bmpHandle = CreateCompatibleBitmap(hdc/*use the window DC not the compat DC*/,work_state->bmpWidth,work_state->bmpHeight);
		if (!work_state->bmpHandle) {
			MessageBox((unsigned)NULL,"Unable to get compatible bitmap","Err",MB_OK);
			return 1;
		}

		work_state->bmpOld = SelectObject(work_state->bmpDC,work_state->bmpHandle);

		ReleaseDC(work_state->hwndMain,hdc);
	}

	/* NTS: Unlike the main bitmap, this time we do NOT select the palette into the icon DC.
	 *      The reason for this is so that, in 256-color mode, to ensure that all StretchDIBits
	 *      calls are forced to use only the 20 default system colors in the Windows UI,
	 *
	 *      While icon resources can have a color palette, CreateIcon() does not allow us to
	 *      provide a color palette and therefore there is no sense in trying to use all
	 *      256 colors in the image anyway, they will just display wrong the instant anything
	 *      else uses the color palette, but the default 20 colors in Windows are always there
	 *      (unless of course you call that API function to unlock them and gain the ability
	 *      to control 254 out of 256 colors in the palette, of course).
	 *
	 *      This is not a big deal in Windows 3.1 where this program has the ability to draw
	 *      it's own minified icon but if we want the icon to appear in the taskbar and ALT+TAB
	 *      in Windows 95 we have to do this. The effect of this code is that even our custom
	 *      drawn minified icon in Windows 3.1 limits itself to 16 colors, which is fine since
	 *      then our minified window doesn't conflict in any way with any other application.
	 *      But in Windows 3.1 we can draw with the palette anyway while minimized.
	 *
	 *      So: Windows 3.1: Select the palette into the bitmap icon, PAL_COLORS.
	 *          Windows 95: Do not select palette, RGB_COLORS */
	{
		HDC hdc = GetDC(work_state->hwndMain);

		work_state->bmpIconDC = CreateCompatibleDC(hdc);
		if (!work_state->bmpIconDC) {
			MessageBox((unsigned)NULL,"Unable to get compatible DC (icon)","Err",MB_OK);
			return 1;
		}

		if (work_state->bmpPalette && !work_state->win95) {
			oldIconPal = SelectPalette(work_state->bmpIconDC,work_state->bmpPalette,TRUE);
			RealizePalette(work_state->bmpDC);
		}

		work_state->bmpIcon = CreateCompatibleBitmap(hdc/*use the window DC not the compat DC*/,work_state->iconWidth,work_state->iconHeight);
		if (!work_state->bmpIcon) {
			MessageBox((unsigned)NULL,"Unable to get compatible bitmap","Err",MB_OK);
			return 1;
		}

		work_state->bmpIconOld = SelectObject(work_state->bmpIconDC,work_state->bmpIcon);

		ReleaseDC(work_state->hwndMain,hdc);
	}
	if (work_state->win95) {
		HDC hdc = GetDC(work_state->hwndMain);

		work_state->bmpIconSmallDC = CreateCompatibleDC(hdc);
		if (!work_state->bmpIconSmallDC) {
			MessageBox((unsigned)NULL,"Unable to get compatible DC (icon)","Err",MB_OK);
			return 1;
		}

		work_state->bmpIconSmall = CreateCompatibleBitmap(hdc/*use the window DC not the compat DC*/,work_state->iconSmallWidth,work_state->iconSmallHeight);
		if (!work_state->bmpIconSmall) {
			MessageBox((unsigned)NULL,"Unable to get compatible bitmap","Err",MB_OK);
			return 1;
		}

		work_state->bmpIconSmallOld = SelectObject(work_state->bmpIconSmallDC,work_state->bmpIconSmall);

		ReleaseDC(work_state->hwndMain,hdc);
	}

	{
		unsigned int p=0;

		/* OK, now read it in! */
		draw_progress(work_state,p,bfr->height);
		while (read_bmp_line(bfr) == 0) {
			convert_scanline(bfr,bfr->scanline,bfr->width);
			load_bmp_scanline(work_state,bfr->current_line,bfr->scanline);
			draw_progress(work_state,++p,bfr->height);
		}

		draw_progress(work_state,0,0);
	}

	/* done reading */
	close_bmp(&bfr);

	if (work_state->bmpPalette) {
		SelectPalette(work_state->bmpDC,oldPal,FALSE);
		oldPal = (HPALETTE)0;
	}

	if (work_state->bmpPalette && !work_state->win95) {
		SelectPalette(work_state->bmpIconDC,oldIconPal,FALSE);
		oldIconPal = (HPALETTE)0;
	}

	/* Create an HICON of the minified image, but only for Windows 95.
	 * We draw our own icon in Windwos 3.1, and Windows 3.1 only allows association of an
	 * icon to a window at the window class level anyway, while Windows 95 allows per-icon
	 * using WM_SETICON. */
	/* NTS: WM_SETICON works for both Win16 and Win32 under Windows 95, 98, and ME.
	 *      Windows XP however ignores Win16 WM_SETICON calls. That means if you run these Win16
	 *      builds under XP, you will only see a generic Windows icon in the taskbar, not the
	 *      icon we're trying to display. Perhaps NTVDM.EXE in Windows XP has no translation
	 *      code for WM_SETICON and Microsoft didn't care enough to support it. After all,
	 *      WM_SETICON didn't exist in Windows 3.1 so why should NTVDM.EXE support it? */
	if (work_state->bmpIconSmall && work_state->iconSmallWidth && work_state->iconSmallHeight && work_state->win95)
		work_state->bmpIconSmallIcon = bitmap2icon(work_state->bmpIconSmall);
	if (work_state->bmpIcon && work_state->win95)
		work_state->bmpIconIcon = bitmap2icon(work_state->bmpIcon);

	if (work_state->bmpIconIcon)
		SendMessage(work_state->hwndMain, WM_SETICON, ICON_BIG, (LPARAM)work_state->bmpIconIcon);
	if (work_state->bmpIconSmallIcon)
		SendMessage(work_state->hwndMain, WM_SETICON, ICON_SMALL, (LPARAM)work_state->bmpIconSmallIcon);

	work_state->isLoading = FALSE;
	work_state->drawReady = TRUE;

	{
		HMENU SysMenu = GetSystemMenu(work_state->hwndMain,FALSE);
		EnableMenuItem(SysMenu,SC_CLOSE,MF_ENABLED);
		EnableMenuItem(SysMenu,IDCSM_INFO,MF_ENABLED);
		EnableMenuItem(SysMenu,IDCSM_DDBDUMP,MF_ENABLED);
	}

	/* we don't need this anymore */
	cleanup_bmpinfoiconraw(work_state);

	/* Windows 95 will never WM_PAINT our window when minimized.
	 * For Windows 95, discard our bmpIcon and bmpSmallIcon bitmaps, we don't need them anymore.
	 * The only thing needed after that point are the HICON resources we created for our window icons. */
	if (work_state->win95) {
		cleanup_bmpiconsmall(work_state);
		cleanup_bmpicon(work_state);
	}

	/* force redraw */
	InvalidateRect(work_state->hwndMain,(unsigned)NULL,FALSE);
	UpdateWindow(work_state->hwndMain);

	while (gmRet=GetMessage(&msg,(unsigned)NULL,0,0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);

		if (work_state->displayModeChangeReinit) {
			retv = APPLOOP_RESTART;
			break;
		}
	}

	/* Before destroying the icons, remove them from the window.
	 * If we leave destroyed icons in the window, the Windows 95 explorer shell might crash at random
	 * drawing a destroyed icon while this code goes into reloading state after display mode change. */
	if (work_state->bmpIconIcon)
		SendMessage(work_state->hwndMain, WM_SETICON, ICON_BIG, (LPARAM)NULL);
	if (work_state->bmpIconSmallIcon)
		SendMessage(work_state->hwndMain, WM_SETICON, ICON_SMALL, (LPARAM)NULL);

	cleanup_bmp(work_state);
	cleanup_bmpiconsmall(work_state);
	cleanup_bmpiconsmallicon(work_state);
	cleanup_bmpicon(work_state);
	cleanup_bmpiconicon(work_state);
	cleanup_bmppalette(work_state);
	work_state->displayModeChangeReinit = FALSE;

	if (gmRet == FALSE) /* WM_QUIT */
		return msg.wParam;

	return retv;
}

int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
	struct wndstate_t FAR *work_state;
	char* bmpfile = NULL;
	HWND hwndMain;
	WNDCLASS wnd;
	int retv;

	probe_dos();
	detect_windows();

	myInstance = hInstance;

	work_state = &the_state;
	wndstate_init(work_state);

	CheckIconSizes(work_state);

	if (windows_mode == WINDOWS_ENHANCED || windows_mode == WINDOWS_NT) {
		if (windows_version >= 0x350) { /* NTS: 3.95 or higher == Windows 95 or Windows NT 4.0 */
			work_state->canBitfields = TRUE; /* we can use BITMAPV4HEADER and BI_BITFIELDS */
			work_state->can32bpp = TRUE; /* can use 32bpp ARGB */
			work_state->can16bpp = TRUE; /* can use 16bpp 5:5:5 */
			work_state->win95 = TRUE;
		}
	}

	/* Windows 95: Ask Windows the "work area" we can use without overlapping the task bar */
	queryDesktopWorkArea(work_state,&work_state->desktopWorkArea);

	/* TODO:  If we assign bmpfile = lpCmdLine, it remains valid through this code but Windows APIs
	 *        sometimes gets corrupted or gibberish strings i.e. CreateWindow() and SetWindowText(),
	 *        especially in real-mode Windows 3.0.
	 *
	 *        Making a copy with strdup() seems to solve this issue for some unknown reason.
	 *
	 *        Why? */
	if (*lpCmdLine) {
		bmpfile = strdup(lpCmdLine);
	}
	else if (windows_version >= 0x30A/*Windows 3.1 or higher*/) {
		bmpfile = CommDlgGetOpenFileName();
		if (!bmpfile)
			return 1;
	}

	if (bmpfile == NULL) {
		MessageBox((unsigned)NULL,"No bitmap specified","Err",MB_OK);
		return 1;
	}

	if (!hPrevInstance) {
		wnd.style = CS_HREDRAW|CS_VREDRAW;
		/* WARNING: If anyone ever tells you to use MakeProcInstance() with RegisterClass, ignore them.
		 *          None of Microsoft's SDK samples do it either for RegisterClass (but they do it for
		 *          DialogBox() though?). What MakeProcInstance() ultimately does is cause the window
		 *          procedure, when it is called, to always get the same DS data segment as the initial
		 *          instance. This code is written to work with the DS segment of each individual instance.
		 *          Older versions of this code had an elaborate Win16 global memory handle array tracking
		 *          system that was completely unnecessary because of this misunderstanding.  */
		wnd.lpfnWndProc = (WNDPROC)WndProc;
		wnd.cbClsExtra = 0;
		wnd.cbWndExtra = 0;
		wnd.hInstance = hInstance;
		wnd.hIcon = (unsigned)NULL; /* do NOT load a class icon so that Windows 3.1 lets us paint our own icon */
		wnd.hCursor = (unsigned)NULL;
		wnd.hbrBackground = (unsigned)NULL;
		wnd.lpszMenuName = NULL;
		wnd.lpszClassName = WndProcClass;

		if (!RegisterClass(&wnd)) {
			MessageBox((unsigned)NULL,"Unable to register Window class","Oops!",MB_OK);
			return 1;
		}
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

	/* first instance already called init on each element */
	work_state->hwndMain = hwndMain;
	work_state->bmpfile = bmpfile;

	{
		HMENU SysMenu = GetSystemMenu(hwndMain,FALSE);
		AppendMenu(SysMenu,MF_SEPARATOR,0,"");
		AppendMenu(SysMenu,MF_STRING,IDCSM_INFO,"Image and display &info");
		AppendMenu(SysMenu,MF_SEPARATOR,0,"");
		AppendMenu(SysMenu,MF_STRING,IDCSM_DDBDUMP,"&Dump DDB to file");
		AppendMenu(SysMenu,MF_SEPARATOR,0,"");
		AppendMenu(SysMenu,MF_STRING,IDCSM_ABOUT,"&About this program");
		EnableMenuItem(SysMenu,SC_CLOSE,MF_DISABLED|MF_GRAYED);
	}

	do {
		retv = AppLoop(work_state,nCmdShow);
	} while (retv < 0);

	return retv;
}

