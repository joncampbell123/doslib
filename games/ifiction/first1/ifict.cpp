
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

#if defined(TARGET_MSDOS)
#include <ext/zlib/zlib.h>
#else
#include <zlib.h>
#endif

#ifndef O_BINARY
#define O_BINARY (0)
#endif

extern "C" {
#include <fmt/minipng/minipng.h>
}

ifeapi_t *ifeapi = &ifeapi_default;

iferect_t IFEScissor;

/* load one PNG into ONE bitmap, that's it */
bool IFELoadPNG(IFEBitmap &bmp,const char *path) {
	bool res = false,transparency = false;
	struct minipng_reader *rdr = NULL;
	unsigned int i;

	if ((rdr=minipng_reader_open(path)) == NULL)
		goto done;
	if (minipng_reader_parse_head(rdr))
		goto done;
	if (rdr->ihdr.width > 2048 || rdr->ihdr.width < 1 || rdr->ihdr.height > 2048 || rdr->ihdr.height < 1)
		goto done;
	if (rdr->ihdr.color_type != 3) /* PNG_COLOR_TYPE_PALETTE = PNG_COLOR_MASK_COLOR | PNG_COLOR_MASK_PALETTE */
		goto done;

	if (rdr->trns != NULL && rdr->trns_size != 0) {
		for (i=0;i < rdr->trns_size;i++) {
			if (!(rdr->trns[i] & 0x80)) {
				transparency = true;
			}
		}
	}

	/* transparency is accomplished by allocating twice the width,
	 * making the right hand side the mask and the left hand the image */

	if (!bmp.alloc_subrects(1))
		goto done;
	if (!bmp.alloc_storage(rdr->ihdr.width * (transparency ? 2u : 1u), rdr->ihdr.height))
		goto done;

	if (rdr->ihdr.bit_depth >= 1 && rdr->ihdr.bit_depth <= 8) {
		if (rdr->plte == NULL)
			goto done;
		if (!bmp.alloc_palette(1u << rdr->ihdr.bit_depth))
			goto done;

		i=0;
		while (i < bmp.palette_alloc && i < rdr->plte_count) {
			bmp.palette[i].r = rdr->plte[i].red;
			bmp.palette[i].g = rdr->plte[i].green;
			bmp.palette[i].b = rdr->plte[i].blue;
			i++;
		}
		while (i < bmp.palette_alloc) {
			bmp.palette[i].r = 0;
			bmp.palette[i].g = 0;
			bmp.palette[i].b = 0;
			i++;
		}

		bmp.palette_size = rdr->plte_count;
	}
	else {
		bmp.free_palette();
	}

	{
		IFEBitmap::subrect &sr = bmp.get_subrect(0);
		sr.has_mask = transparency;
		sr.r.w = rdr->ihdr.width;
		sr.r.h = rdr->ihdr.height;
		sr.r.x = 0;
		sr.r.y = 0;

		if (rdr->ihdr.bit_depth == 8) {
			unsigned char *ptr = bmp.bitmap;

			for (i=0;i < bmp.height;i++) {
				minipng_reader_read_idat(rdr,ptr,1); /* pad byte */
				minipng_reader_read_idat(rdr,ptr,rdr->ihdr.width); /* row */
				ptr += bmp.stride;
			}
		}
		else {
			memset(bmp.bitmap,0,bmp.stride * bmp.height);
		}

		if (transparency) {
			/* NTS: Remember the bitmap allocated is twice the width of the PNG, mask starts on right hand side */
			unsigned int x;
			unsigned char *ptr = bmp.bitmap;
			unsigned char *msk = ptr + rdr->ihdr.width;

			for (i=0;i < bmp.height;i++) {
				for (x=0;x < rdr->ihdr.width;x++) {
					if (ptr[x] < rdr->trns_size && !(rdr->trns[ptr[x]] & 0x80)) {
						msk[x] = 0xFF; /* transparent ((dst AND msk) + src) == dst */
						ptr[x] = 0x00; /* for this to work, make sure src == 0 */
					}
					else {
						msk[x] = 0x00; /* opaque ((dst AND msk) + src) == src */
					}
				}
				ptr += bmp.stride;
				msk += bmp.stride;
			}
		}

		res = true;
	}

done:
        minipng_reader_close(&rdr);
	return res;
}

void IFEAddScreenUpdate(int x1,int y1,int x2,int y2) {
	if (x1 < IFEScissor.x)
		x1 = IFEScissor.x;
	if (y1 < IFEScissor.y)
		y1 = IFEScissor.y;
	if (x2 > (IFEScissor.x+IFEScissor.w))
		x2 = (IFEScissor.x+IFEScissor.w);
	if (y2 > (IFEScissor.y+IFEScissor.h))
		y2 = (IFEScissor.y+IFEScissor.h);

	ifeapi->AddScreenUpdate(x1,y1,x2,y2);
}

void IFEResetScissorRect(void) {
	ifevidinfo_t* vi = ifeapi->GetVidInfo();

	IFEScissor.x = IFEScissor.y = 0;
	IFEScissor.w = (int)vi->width;
	IFEScissor.h = (int)vi->height;
}

void IFESetScissorRect(int x1,int y1,int x2,int y2) {
	ifevidinfo_t* vi = ifeapi->GetVidInfo();

	if (x1 < 0) x1 = 0;
	if (y1 < 0) y1 = 0;
	if ((unsigned int)x2 > vi->width) x2 = vi->width;
	if ((unsigned int)y2 > vi->height) y2 = vi->height;
	if (x1 > x2) x1 = x2;
	if (y1 > y2) y1 = y2;

	IFEScissor.x = x1;
	IFEScissor.y = y1;
	IFEScissor.w = x2-x1;
	IFEScissor.h = y2-y1;
}

void IFECompleteVideoInit(void) {
	ifevidinfo_t* vi = ifeapi->GetVidInfo();

	IFEScissor.x = IFEScissor.y = 0;
	IFEScissor.w = (int)vi->width;
	IFEScissor.h = (int)vi->height;
}

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

void IFEBlankScreen(void) {
	unsigned char *firstrow;
	unsigned int y,w,h;
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
		memset(row,0,w);
	}

	ifeapi->EndScreenDraw();
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

static bool IFEBitBlt_clipcheck(int &dx,int &dy,int &w,int &h,int &sx,int &sy,const IFEBitmap &bmp,iferect_t &scissor,const bool has_mask) {
	/* BMP with no storage? caller wants to draw bitmaps with negative dimensions? Exit now. Assume scissor rect is valid. */
	if (bmp.bitmap == NULL || w <= 0 || h <= 0 || scissor.w <= 0 || scissor.h <= 0) return false;

	/* simplify code below by subtracting scissor rect top/left from dx/dy here, to add it back later */
	dx -= scissor.x;
	dy -= scissor.y;

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

	int actual_w = has_mask ? (w * 2) : w;

	if ((unsigned int)(sx+actual_w) > bmp.width)
		w = bmp.width - sx;
	if ((unsigned int)(sy+h) > bmp.height)
		h = bmp.height - sy;
	if ((unsigned int)(dx+w) > (unsigned int)scissor.w)
		w = scissor.w - dx;
	if ((unsigned int)(dy+h) > (unsigned int)scissor.h)
		h = scissor.h - dy;

	if (w <= 0 || h <= 0) return false;

	/* remove adjustment above */
	dx += scissor.x;
	dy += scissor.y;

#if 1//DEBUG
	if ((unsigned int)(sx+actual_w) > bmp.width && (unsigned int)(sy+h) > bmp.height && (unsigned int)(dx+w) > (unsigned int)scissor.w && (unsigned int)(dy+h) > (unsigned int)scissor.h)
		IFEFatalError("IFEBitBlt bug check fail line %d: sx=%d sy=%d dx=%d dy=%d w=%d h=%d bw=%d bh=%d scw=%d sch=%d",
			__LINE__,sx,sy,dx,dy,actual_w,h,bmp.width,bmp.height,scissor.w,scissor.h);
#endif

	return true;
}

void IFEBitBlt(int dx,int dy,int w,int h,int sx,int sy,const IFEBitmap &bmp) {
	ifevidinfo_t* vi = ifeapi->GetVidInfo(); /* cannot return NULL */

	if (!IFEBitBlt_clipcheck(dx,dy,w,h,sx,sy,bmp,IFEScissor,false/*no mask*/)) return;

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

static inline void memcpymask(unsigned char *dst,const unsigned char *src,const unsigned char *msk,unsigned int w) {
	while (w >= 4) {
		*((uint32_t*)dst) = (*((uint32_t*)dst) & *((uint32_t*)msk)) + *((uint32_t*)src);
		dst += 4;
		msk += 4;
		src += 4;
		w -= 4;
	}
	while (w > 0) {
		*dst = (*dst & *msk) + *src;
		dst++;
		msk++;
		src++;
		w--;
	}
}

void IFETBitBlt(int dx,int dy,int w,int h,int sx,int sy,const IFEBitmap &bmp) {
	int orig_w = w;

	ifevidinfo_t* vi = ifeapi->GetVidInfo(); /* cannot return NULL */

	if (!IFEBitBlt_clipcheck(dx,dy,w,h,sx,sy,bmp,IFEScissor,true/*has mask*/)) return;

	if (!ifeapi->BeginScreenDraw()) IFEFatalError("IFEBitBlt unable to begin screen draw");
	if (vi->buf_first_row == NULL) IFEFatalError("IFEBitBlt video output buffer first row == NULL");

	const unsigned char *src = bmp.row(sy,sx);
	if (src == NULL) IFEFatalError("IFEBitBlt BMP row returned NULL, line %d",__LINE__); /* What? Despite all validation in clipcheck? */
	const unsigned char *msk = src + orig_w;

	unsigned char *dst = vi->buf_first_row + (dy * vi->buf_pitch) + dx; /* remember, buf_pitch is signed because Windows 3.1 upside down DIBs to allow negative pitch */

	while (h > 0) {
		memcpymask(dst,src,msk,w);
		dst += vi->buf_pitch; /* NTS: buf_pitch is negative if Windows 3.1 */
		src += bmp.stride;
		msk += bmp.stride;
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
		IFEAddScreenUpdate(20,20,20+bmp.width,20+bmp.height);
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
					IFEAddScreenUpdate(px,py,px+bmp.width,py+bmp.height);
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

		/* test clipping by letting the user mouse around with it, scissor rect */
		IFESetScissorRect(100,100,540,380);
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					px = ms->x;
					py = ms->y;

					IFEBitBlt(/*dest*/px,py,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
					IFEAddScreenUpdate(px,py,px+bmp.width,py+bmp.height);
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

		/* test clipping by letting the user mouse around with it, scissor rect */
		IFESetScissorRect(200,200,440,280);
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					px = ms->x;
					py = ms->y;

					IFEBitBlt(/*dest*/px,py,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
					IFEAddScreenUpdate(px,py,px+bmp.width,py+bmp.height);
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

	{
		IFEBitmap bmp,tbmp;

		IFEResetScissorRect();

		if (!IFELoadPNG(bmp,"test1.png"))
			IFEFatalError("Unable to load PNG");
		if (bmp.row(0) == NULL)
			IFEFatalError("BMP storage failure, row");
		if (bmp.palette == NULL)
			IFEFatalError("PNG did not have palette");

		if (!IFELoadPNG(tbmp,"woo1.png"))
			IFEFatalError("Unable to load PNG");
		if (tbmp.row(0) == NULL)
			IFEFatalError("BMP storage failure, row");
		if (tbmp.palette == NULL)
			IFEFatalError("PNG did not have palette");
		if (tbmp.subrects_alloc < 1)
			IFEFatalError("PNG without subrect");
		if (!tbmp.get_subrect(0).has_mask)
			IFEFatalError("PNG should have transparency");

		/* blank screen to avoid palette flash */
		IFEBlankScreen();
		ifeapi->UpdateFullScreen();
		ifeapi->SetPaletteColors(0,bmp.palette_size,bmp.palette);

		IFEBitBlt(/*dest*/0,0,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
		IFEBitBlt(/*dest*/40,40,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
		IFETBitBlt(/*dest*/40,240,/*width,height*/tbmp.width/2/*image is 2x the width for mask*/,tbmp.height,/*source*/0,0,tbmp);
		ifeapi->UpdateFullScreen();

		ifeapi->ResetTicks(ifeapi->GetTicks());
		while (ifeapi->GetTicks() < 3000) {
			if (ifeapi->UserWantsToQuit()) IFENormalExit();
			ifeapi->WaitEvent(1);
		}

		/* test clipping by letting the user mouse around with it */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEBitBlt(/*dest*/px,py,/*width,height*/tbmp.width/2,tbmp.height,/*source*/px,py,bmp);
					IFEAddScreenUpdate(px,py,px+(tbmp.width/2),py+tbmp.height);
					px = ms->x;
					py = ms->y;
					IFETBitBlt(/*dest*/px,py,/*width,height*/tbmp.width/2/*image is 2x the width for mask*/,tbmp.height,/*source*/0,0,tbmp);
					IFEAddScreenUpdate(px,py,px+(tbmp.width/2),py+tbmp.height);

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

	/* blank screen to avoid palette flash */
	IFEBlankScreen();
	ifeapi->UpdateFullScreen();

	IFETestRGBPalette();

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

