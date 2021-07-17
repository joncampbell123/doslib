
#if defined(USE_WIN32)
# include <windows.h>
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
#include "bitmap.h"
#include "palette.h"

ifeapi_t *ifeapi = &ifeapi_default;

void IFETestRGBPalette() {
	IFEPaletteEntry pal[256];
	unsigned int i;

	for (i=0;i < 64;i++) {
		pal[i].r = (unsigned char)(i*4u);
		pal[i].g = (unsigned char)(i*4u);
		pal[i].b = (unsigned char)(i*4u);

		pal[i+64u].r = (unsigned char)(i*4u);
		pal[i+64u].g = 0u;
		pal[i+64u].b = 0u;

		pal[i+128u].r = 0u;
		pal[i+128u].g = (unsigned char)(i*4u);
		pal[i+128u].b = 0u;

		pal[i+192u].r = 0u;
		pal[i+192u].g = 0u;
		pal[i+192u].b = (unsigned char)(i*4u);
	}

	ifeapi->SetPaletteColors(0,256,pal);
}

void IFETestRGBPalettePattern(void) {
	unsigned char *firstrow;
	unsigned int x,y,w,h;
	ifevidinfo_t* vif;
	int pitch;

	if (!ifeapi->BeginScreenDraw())
		IFEFatalError("BeginScreenDraw TestRGBPalettePattern");
	if ((vif=ifeapi->GetVidInfo()) == NULL)
		IFEFatalError("GetVidInfo() == NULL");
	if ((firstrow=vif->buf_first_row) == NULL)
		IFEFatalError("ScreenDrawPointer==NULL TestRGBPalettePattern");

	w = vif->width;
	h = vif->height;
	pitch = vif->buf_pitch;
	for (y=0;y < h;y++) {
		unsigned char *row = firstrow + ((int)y * pitch);
		for (x=0;x < w;x++) {
			if ((x & 0xF0u) != 0xF0u)
				row[x] = (unsigned char)(y & 0xFFu);
			else
				row[x] = 0;
		}
	}

	ifeapi->EndScreenDraw();
}

void IFETestRGBPalettePattern2(void) {
	unsigned char *firstrow;
	unsigned int x,y,w,h;
	ifevidinfo_t* vif;
	int pitch;

	if (!ifeapi->BeginScreenDraw())
		IFEFatalError("BeginScreenDraw TestRGBPalettePattern");
	if ((vif=ifeapi->GetVidInfo()) == NULL)
		IFEFatalError("GetVidInfo() == NULL");
	if ((firstrow=vif->buf_first_row) == NULL)
		IFEFatalError("ScreenDrawPointer==NULL TestRGBPalettePattern");

	w = vif->width;
	h = vif->height;
	pitch = vif->buf_pitch;
	for (y=0;y < h;y++) {
		unsigned char *row = firstrow + ((int)y * pitch);
		for (x=0;x < w;x++) {
			row[x] = (unsigned char)(x ^ y);
		}
	}

	ifeapi->EndScreenDraw();
}

static bool IFEBitBlt_clipcheck(int &dx,int &dy,int &w,int &h,int &sx,int &sy,const IFEBitmap &bmp,ifevidinfo_t* vi) {
	/* BMP with no storage? caller wants to draw bitmaps with negative dimensions? Exit now */
	if (bmp.bitmap == NULL || w <= 0 || h <= 0) return false;

	/* upper top/left source/dest clipping */
	if (sx < 0) { w += sx; dx -= sx; sx = 0; }
	if (sy < 0) { h += sy; dy -= sy; sy = 0; }
	if (dx < 0) { w += dx; sx -= dx; dx = 0; }
	if (dy < 0) { h += dy; sy -= dy; dy = 0; }
	if (w <= 0 || h <= 0) return false;

	/* assume by this point, sx/sy/dx/dy are >= 0, w/h are > 0 */
#if 1//DEBUG
	if (sx < 0 || sy < 0 || dx < 0 || dy < 0)
		IFEFatalError("IFEBitBlt bug check fail line %d: sx=%d sy=%d dx=%d dy=%d w=%d h=%d",__LINE__,sx,sy,dx,dy,w,h);
#endif

	if ((unsigned int)(sx+w) > bmp.width)
		w = bmp.width - sx;
	if ((unsigned int)(sy+h) > bmp.height)
		h = bmp.height - sy;
	if ((unsigned int)(dx+w) > vi->width)
		w = vi->width - dx;
	if ((unsigned int)(dy+h) > vi->height)
		h = vi->height - dy;

	if (w <= 0 || h <= 0) return false;

#if 1//DEBUG
	if ((unsigned int)(sx+w) > bmp.width && (unsigned int)(sy+h) > bmp.height && (unsigned int)(dx+w) > vi->width && (unsigned int)(dy+h) > vi->height)
		IFEFatalError("IFEBitBlt bug check fail line %d: sx=%d sy=%d dx=%d dy=%d w=%d h=%d bw=%d bh=%d vw=%d vh=%d",
			__LINE__,sx,sy,dx,dy,w,h,bmp.width,bmp.height,vi->width,vi->height);
#endif

	return true;
}

void IFEBitBlt(int dx,int dy,int w,int h,int sx,int sy,const IFEBitmap &bmp) {
	ifevidinfo_t* vi = ifeapi->GetVidInfo(); /* cannot return NULL */

	if (!IFEBitBlt_clipcheck(dx,dy,w,h,sx,sy,bmp,vi)) return;

	if (!ifeapi->BeginScreenDraw()) IFEFatalError("IFEBitBlt unable to begin screen draw");
	if (vi->buf_first_row == NULL) IFEFatalError("IFEBitBlt video output buffer first row == NULL");

	const unsigned char *src = bmp.row(sy,sx);
	if (src == NULL) IFEFatalError("IFEBitBlt BMP row returned NULL, line %d",__LINE__); /* What? Despite all validation in clipcheck? */

	unsigned char *dst = vi->buf_first_row + (dy * vi->buf_pitch) + dx; /* remember, buf_pitch is signed because Windows 3.1 upside down DIBs to allow negative pitch */

	while (h > 0) {
		memcpy(dst,src,w);
		dst += vi->buf_pitch; /* NTS: buf_pitch is negative if Windows 3.1 */
		src += bmp.stride;
		h--;
	}

	ifeapi->EndScreenDraw();
}

void IFENormalExit(void) {
#if defined(USE_DOSLIB)
	IFE_win95_tf_hang_check();
#endif

	ifeapi->ShutdownVideo();
	exit(0);
}

#if defined(USE_WIN32)
int PASCAL WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance/*doesn't mean anything in Win32*/,LPSTR lpCmdLine,int nCmdShow) {
	if (!priv_IFEWin32Init(hInstance,hPrevInstance,lpCmdLine,nCmdShow))
		return 1;
#else
int main(int argc,char **argv) {
	if (!priv_IFEMainInit(argc,argv))
		return 1;
#endif

	ifeapi->InitVideo();

	ifeapi->ResetTicks(ifeapi->GetTicks());
	while (ifeapi->GetTicks() < 1000) {
		if (ifeapi->UserWantsToQuit()) IFENormalExit();
		ifeapi->WaitEvent(100);
	}

	IFETestRGBPalette();

	IFETestRGBPalettePattern();
	ifeapi->UpdateFullScreen();
	ifeapi->ResetTicks(ifeapi->GetTicks());
	while (ifeapi->GetTicks() < 3000) {
		if (ifeapi->UserWantsToQuit()) IFENormalExit();
		ifeapi->WaitEvent(100);
	}

	{
		unsigned char *row;
		unsigned int x,y;
		IFEBitmap bmp;

		if (!bmp.alloc_storage(320,200))
			IFEFatalError("BMP alloc fail");
		if (!bmp.alloc_storage(352,288))
			IFEFatalError("BMP realloc fail");
		if (!bmp.alloc_subrects(64))
			IFEFatalError("BMP alloc subrect fail");
		if (!bmp.alloc_subrects(128))
			IFEFatalError("BMP realloc subrect fail");
		if (bmp.row(0) == NULL)
			IFEFatalError("BMP storage failure, row");

		for (y=0;y < bmp.height;y++) {
			row = bmp.rowfast(y);
			for (x=0;x < bmp.width;x++)
				row[x] = (unsigned char)(x ^ y);
		}

		IFEBitBlt(/*dest*/20,20,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
		ifeapi->AddScreenUpdate(20,20,20+bmp.width,20+bmp.height);
		ifeapi->UpdateScreen();

		/* test clipping by letting the user mouse around with it */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					px = ms->x;
					py = ms->y;

					IFEBitBlt(/*dest*/px,py,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
					ifeapi->AddScreenUpdate(px,py,px+bmp.width,py+bmp.height);
					ifeapi->UpdateScreen();
				}

				if (ms->status & IFEMouseStatus_LBUTTON) {
					if (lx == -999 && ly == -999) {
						lx = ms->x;
						ly = ms->y;
					}
				}
				else {
					if (abs(lx - ms->x) < 8 && abs(ly - ms->y) < 8) /* lbutton released without moving */
						break;

					lx = ly = -999;
				}

				if (ifeapi->UserWantsToQuit()) IFENormalExit();
				ifeapi->WaitEvent(1);
			} while (1);
		}
	}

	IFETestRGBPalettePattern2();
	ifeapi->ResetTicks(ifeapi->GetTicks());
	{
		int py = -1;
		while (ifeapi->GetTicks() < 3000) {
			int y = (ifeapi->GetTicks() * 480) / 3000;
			if (py != y) {
				ifeapi->AddScreenUpdate(0,y,640,y); // same line, isn't supposed to do anything
				ifeapi->AddScreenUpdate(y/2,y,y + 480,y+1);
				ifeapi->AddScreenUpdate(y/4,600-y,(y/2) + 240,(600-y)+1);
				ifeapi->UpdateScreen();
				py = y;
			}
			if (ifeapi->UserWantsToQuit()) IFENormalExit();
			ifeapi->WaitEvent(1);
		}
	}
	IFETestRGBPalettePattern();
	{
		int py = -1;
		while (ifeapi->GetTicks() < 4000) {
			int y = (((ifeapi->GetTicks()-3000) * (480 / 64)) / 1000) * 64;
			if (py != y) {
				ifeapi->AddScreenUpdate(0,y,640,y + 32);
				ifeapi->UpdateScreen();
				py = y;
			}
			if (ifeapi->UserWantsToQuit()) IFENormalExit();
			ifeapi->WaitEvent(1);
		}
	}
	while (ifeapi->GetTicks() < 5000) {
		if (ifeapi->UserWantsToQuit()) IFENormalExit();
		ifeapi->WaitEvent(100);
	}

	IFENormalExit();
	return 0;
}

