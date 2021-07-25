
#if defined(USE_WIN32)
# include <windows.h>
#endif

#include <stdio.h>
#include <assert.h>
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

IFEBitmap* IFEscrbmp = NULL;

IFEBitmap &IFEGetScreenBitmap(void) {
	if (IFEscrbmp != NULL)
		return *IFEscrbmp;

	IFEFatalError("IFEGetScreenBitmap == NULL");
	abort();
}

IFEBitmap IFEcursor_arrow;
IFEBitmap IFEcursor_wait;

struct IFEcursor_state_t {
	int		x;
	int		y;
	IFEBitmap*	bitmap;
	size_t		bitmap_sr;
	bool		saved_valid;
	bool		show_state;
	bool		host_replacement; /* host platform is providing a cursor on our behalf, no need to draw */
	int		hide_draw;

	IFEBitmap	saved;

	IFEcursor_state_t();
};

IFEcursor_state_t::IFEcursor_state_t() {
	x = -999;
	y = -999;
	bitmap = NULL;
	bitmap_sr = 0;
	host_replacement = false;
	saved_valid = false;
	show_state = false;
	hide_draw = 0;
}

IFEcursor_state_t	priv_IFEcursor;

void IFESetCursor(IFEBitmap* new_cursor,size_t sr=0);

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
	if (!bmp.alloc_storage(rdr->ihdr.width, rdr->ihdr.height, transparency ? IFEBitmap::IMT_TRANSPARENT_MASK : IFEBitmap::IMT_OPAQUE))
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
		sr.reset();
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
			unsigned char *msk = ptr + bmp.get_transparent_mask_offset();

			IFEDBG("stride=%u width=%u offset=%u",
				bmp.stride,bmp.width,bmp.get_transparent_mask_offset());
			assert((bmp.get_transparent_mask_offset() + bmp.width) <= abs(bmp.stride));

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
	if (x1 < IFEscrbmp->scissor.x)
		x1 = IFEscrbmp->scissor.x;
	if (y1 < IFEscrbmp->scissor.y)
		y1 = IFEscrbmp->scissor.y;
	if (x2 > (IFEscrbmp->scissor.x+IFEscrbmp->scissor.w))
		x2 = (IFEscrbmp->scissor.x+IFEscrbmp->scissor.w);
	if (y2 > (IFEscrbmp->scissor.y+IFEscrbmp->scissor.h))
		y2 = (IFEscrbmp->scissor.y+IFEscrbmp->scissor.h);

	ifeapi->AddScreenUpdate(x1,y1,x2,y2);
}

void IFEResetScissorRect(void) {
	IFEscrbmp->reset_scissor_rect();
}

void IFESetScissorRect(int x1,int y1,int x2,int y2) {
	IFEscrbmp->set_scissor_rect(x1,y1,x2,y2);
}

void IFECompleteVideoInit(void) {
	IFEscrbmp = ifeapi->GetScreenBitmap();
	if (IFEscrbmp == NULL)
		IFEFatalError("Video code failed to provide screen bitmap");
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

bool IFELockSurface(IFEBitmap &scrbmp) {
	if (!scrbmp.lock_surface()) {
		IFEDBG("IFELockSurface cannot lock surface");
		return false;
	}

	if (scrbmp.bitmap_first_row == NULL) {
		IFEDBG("IFELockSurface lock did not provide pointer");

		if (!scrbmp.unlock_surface())
			IFEDBG("IFELockSurface cannot unlock surface after failure to provide pointer with lock");

		return false;
	}

	return true;
}

bool IFEUnlockSurface(IFEBitmap &scrbmp) {
	if (!scrbmp.unlock_surface())
		IFEFatalError("IFEUnlockSurface cannot unlock surface");

	return true;
}

void IFEBlankScreen(IFEBitmap &dbmp) {
	if (!IFELockSurface(dbmp)) return;

	unsigned int y,h=(unsigned int)(dbmp.height);
	unsigned char *row = dbmp.row(0);

	for (y=0;y < h;y++) {
		memset(row,0,dbmp.width);
		row += dbmp.stride;
	}

	IFEUnlockSurface(dbmp);
}

void IFETestRGBPalettePattern(IFEBitmap &dbmp) {
	if (!IFELockSurface(dbmp)) return;

	unsigned int x,y,w=(unsigned int)(dbmp.width),h=(unsigned int)(dbmp.height);
	unsigned char *row = dbmp.row(0);

	for (y=0;y < h;y++) {
		for (x=0;x < w;x++) {
			if ((x & 0xF0u) != 0xF0u)
				row[x] = (unsigned char)(y & 0xFFu);
			else
				row[x] = 0;
		}

		row += dbmp.stride;
	}

	IFEUnlockSurface(dbmp);
}

void IFETestRGBPalettePattern2(IFEBitmap &dbmp) {
	if (!IFELockSurface(dbmp)) return;

	unsigned int x,y,w=(unsigned int)(dbmp.width),h=(unsigned int)(dbmp.height);
	unsigned char *row = dbmp.row(0);

	for (y=0;y < h;y++) {
		for (x=0;x < w;x++)
			row[x] = (unsigned char)(x ^ y);

		row += dbmp.stride;
	}

	IFEUnlockSurface(dbmp);
}

/* in: destination x,y,w,h
 *     source x,y,w,h
 *     scissor rect to clip to in dest
 * out: destination x,y,w,h (modified)
 *      source x,y (modified) */
static bool IFEBitBlt_clipcheck(int &dx,int &dy,int &dw,int &dh,int &sx,int &sy,const int sw,const int sh,const iferect_t &scissor) {
	/* BMP with no storage? caller wants to draw bitmaps with negative dimensions? Exit now. Assume scissor rect is valid. */
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || scissor.w <= 0 || scissor.h <= 0) return false;

	/* simplify code below by subtracting scissor rect top/left from dx/dy here, to add it back later */
	dx -= scissor.x;
	dy -= scissor.y;

	/* upper top/left source/dest clipping */
	if (sx < 0) { dw += sx; dx -= sx; sx = 0; }
	if (sy < 0) { dh += sy; dy -= sy; sy = 0; }
	if (dx < 0) { dw += dx; sx -= dx; dx = 0; }
	if (dy < 0) { dh += dy; sy -= dy; dy = 0; }
	if (dw <= 0 || dh <= 0) return false;

	/* assume by this point, sx/sy/dx/dy are >= 0, w/h are > 0 */
#if 1//DEBUG
	if (sx < 0 || sy < 0 || dx < 0 || dy < 0)
		IFEFatalError("IFEBitBlt bug check fail line %d: sx=%d sy=%d dx=%d dy=%d dw=%d dh=%d",__LINE__,sx,sy,dx,dy,dw,dh);
#endif

	if ((unsigned int)(sx+dw) > (unsigned int)sw)
		dw = sw - sx;
	if ((unsigned int)(sy+dh) > (unsigned int)sh)
		dh = sh - sy;
	if ((unsigned int)(dx+dw) > (unsigned int)scissor.w)
		dw = scissor.w - dx;
	if ((unsigned int)(dy+dh) > (unsigned int)scissor.h)
		dh = scissor.h - dy;

	if (dw <= 0 || dh <= 0) return false;

	/* remove adjustment above */
	dx += scissor.x;
	dy += scissor.y;

#if 1//DEBUG
	if ((unsigned int)(sx+dw) > (unsigned int)sw && (unsigned int)(sy+dh) > (unsigned int)sh &&
		(unsigned int)(dx+dw) > (unsigned int)scissor.w && (unsigned int)(dy+dh) > (unsigned int)scissor.h)
		IFEFatalError("IFEBitBlt bug check fail line %d: sx=%d sy=%d dx=%d dy=%d dw=%d dh=%d sw=%d sh=%d scw=%d sch=%d",
			__LINE__,sx,sy,dx,dy,dw,dh,sw,sh,scissor.w,scissor.h);
#endif

	return true;
}

/* draw a rectangle, region x1 <= x < x2, y1 <= y < y2 */
void IFEFillRect(IFEBitmap &dbmp,int x1,int y1,int x2,int y2,const uint8_t color) {
	if (x1 >= x2 || y1 >= y2) return;
	if (x1 < dbmp.scissor.x) x1 = dbmp.scissor.x;
	if (y1 < dbmp.scissor.y) y1 = dbmp.scissor.y;
	if (x2 > (dbmp.scissor.x+dbmp.scissor.w)) x2 = (dbmp.scissor.x+dbmp.scissor.w);
	if (y2 > (dbmp.scissor.y+dbmp.scissor.h)) y2 = (dbmp.scissor.y+dbmp.scissor.h);

	int dw = x2 - x1;
	int dh = y2 - y1;
	if (dw <= 0 || dh <= 0) return;

	if (IFELockSurface(dbmp)) {
		switch (dbmp.image_type) {
			case IFEBitmap::IMT_TRANSPARENT_MASK:
				{
					unsigned char *row = dbmp.row(y1,x1);
					unsigned char *msk = row + dbmp.get_transparent_mask_offset();

					do {
						memset(row,color,(unsigned int)dw);
						memset(msk,0x00,(unsigned int)dw);
						row += dbmp.stride;
						msk += dbmp.stride;
					} while ((--dh) > 0);
				}
				break;
			default:
				{
					unsigned char *row = dbmp.row(y1,x1);

					do {
						memset(row,color,(unsigned int)dw);
						row += dbmp.stride;
					} while ((--dh) > 0);
				}
				break;
		}

		IFEUnlockSurface(dbmp);
	}
}

/* draw a rectangle, region x1 <= x < x2, y1 <= y < y2 */
void IFETFillRect(IFEBitmap &dbmp,int x1,int y1,int x2,int y2) {
	if (x1 >= x2 || y1 >= y2) return;
	if (x1 < dbmp.scissor.x) x1 = dbmp.scissor.x;
	if (y1 < dbmp.scissor.y) y1 = dbmp.scissor.y;
	if (x2 > (dbmp.scissor.x+dbmp.scissor.w)) x2 = (dbmp.scissor.x+dbmp.scissor.w);
	if (y2 > (dbmp.scissor.y+dbmp.scissor.h)) y2 = (dbmp.scissor.y+dbmp.scissor.h);

	int dw = x2 - x1;
	int dh = y2 - y1;
	if (dw <= 0 || dh <= 0) return;

	if (IFELockSurface(dbmp)) {
		switch (dbmp.image_type) {
			case IFEBitmap::IMT_TRANSPARENT_MASK:
				{
					unsigned char *row = dbmp.row(y1,x1);
					unsigned char *msk = row + dbmp.get_transparent_mask_offset();

					do {
						memset(row,0x00,(unsigned int)dw);
						memset(msk,0xFF,(unsigned int)dw);
						row += dbmp.stride;
						msk += dbmp.stride;
					} while ((--dh) > 0);
				}
				break;
			default:
				break;
		}

		IFEUnlockSurface(dbmp);
	}
}

/* draws a line rect at x1,y1 to x2-1,y2-1.
 * If combined with fill rect, the line rect will draw on the edge (outermost pixels) of
 * the fill rect. */
void IFELineRect(IFEBitmap &dbmp,int x1,int y1,int x2,int y2,const uint8_t color,const int thickness=1) {
	/* +----------------------+
	 * |         (1)          |
	 * +---+--------------+---+
	 * |   |              |   |
	 * |   |              |   |
	 * |(3)|              |(4)|
	 * |   |              |   |
	 * |   |              |   |
	 * +---+--------------+---+
	 * |         (2)          |
	 * +----------------------+ */
	IFEFillRect(dbmp, x1,            y1,            x2,            y1+thickness,  color);
	IFEFillRect(dbmp, x1,            y2-thickness,  x2,            y2,            color);
	IFEFillRect(dbmp, x1,            y1+thickness,  x1+thickness,  y2-thickness,  color);
	IFEFillRect(dbmp, x2-thickness,  y1+thickness,  x2,            y2-thickness,  color);
}

void IFEBitBlt(IFEBitmap &dbmp,int dx,int dy,int w,int h,int sx,int sy,IFEBitmap &sbmp) {
	if (!IFEBitBlt_clipcheck(dx,dy,w,h,sx,sy,(int)sbmp.width,(int)sbmp.height,dbmp.scissor)) return;

	if (!(sbmp.image_type == IFEBitmap::IMT_OPAQUE || sbmp.image_type == IFEBitmap::IMT_TRANSPARENT_MASK))
		return;

	if (IFELockSurface(dbmp)) {
		if (IFELockSurface(sbmp)) {
			switch (dbmp.image_type) {
				case IFEBitmap::IMT_TRANSPARENT_MASK:
					{
						const unsigned char *src = sbmp.row(sy,sx);
						const unsigned char *sms = src + sbmp.get_transparent_mask_offset();

						unsigned char *dst = dbmp.row(dy,dx);
						unsigned char *dms = dst + dbmp.get_transparent_mask_offset();

						while (h > 0) {
							memcpy(dst,src,w);
							dst += dbmp.stride; /* NTS: stride is negative if Windows 3.1 */
							src += sbmp.stride;
							memcpy(dms,sms,w);
							dms += dbmp.stride; /* NTS: stride is negative if Windows 3.1 */
							sms += sbmp.stride;
							h--;
						}
					}
					break;
				default:
					{
						const unsigned char *src = sbmp.row(sy,sx);
						unsigned char *dst = dbmp.row(dy,dx);

						while (h > 0) {
							memcpy(dst,src,w);
							dst += dbmp.stride; /* NTS: stride is negative if Windows 3.1 */
							src += sbmp.stride;
							h--;
						}
					}
					break;
			}

			IFEUnlockSurface(sbmp);
		}

		IFEUnlockSurface(dbmp);
	}
}

static inline void memcpyand(unsigned char *dst,const unsigned char *src,unsigned int w) {
	while (w >= 4) {
		*((uint32_t*)dst) &= *((uint32_t*)src);
		dst += 4;
		src += 4;
		w -= 4;
	}
	while (w > 0) {
		*dst &= *src;
		dst++;
		src++;
		w--;
	}
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
		*dst = (unsigned char)((*dst & *msk) + *src);
		dst++;
		msk++;
		src++;
		w--;
	}
}

void IFETBitBlt(IFEBitmap &dbmp,int dx,int dy,int w,int h,int sx,int sy,IFEBitmap &sbmp) {
	if (!IFEBitBlt_clipcheck(dx,dy,w,h,sx,sy,(int)sbmp.width,(int)sbmp.height,dbmp.scissor)) return;

	if (!(sbmp.image_type == IFEBitmap::IMT_TRANSPARENT_MASK))
		return;

	if (IFELockSurface(dbmp)) {
		if (IFELockSurface(sbmp)) {
			switch (dbmp.image_type) {
				case IFEBitmap::IMT_TRANSPARENT_MASK:
					{
						const unsigned char *src = sbmp.row(sy,sx);
						const unsigned char *sms = src + sbmp.get_transparent_mask_offset();

						unsigned char *dst = dbmp.row(dy,dx);
						unsigned char *dms = dst + dbmp.get_transparent_mask_offset();

						while (h > 0) {
							memcpymask(dst,src,sms,w); /* mask combine pixels given source mask */
							dst += dbmp.stride; /* NTS: buf_pitch is negative if Windows 3.1 */
							src += sbmp.stride;
							memcpyand(dms,sms,w); /* then combine masks */
							dms += dbmp.stride;
							sms += sbmp.stride;
							h--;
						}
					}
					break;
				default:
					{
						const unsigned char *src = sbmp.row(sy,sx);
						const unsigned char *msk = src + sbmp.get_transparent_mask_offset();

						unsigned char *dst = dbmp.row(dy,dx);

						while (h > 0) {
							memcpymask(dst,src,msk,w);
							dst += dbmp.stride; /* NTS: buf_pitch is negative if Windows 3.1 */
							src += sbmp.stride;
							msk += sbmp.stride;
							h--;
						}
					}
					break;
			}

			IFEUnlockSurface(sbmp);
		}

		IFEUnlockSurface(dbmp);
	}
}

void priv_IFEUndrawCursor(void) {
	if (priv_IFEcursor.bitmap == NULL) return;

	if (priv_IFEcursor.saved_valid) {
		const IFEBitmap::subrect &r = priv_IFEcursor.bitmap->get_subrect(priv_IFEcursor.bitmap_sr);

		IFEBitBlt(	*IFEscrbmp,
				/*dest*/priv_IFEcursor.x+r.offset_x,priv_IFEcursor.y+r.offset_y,
				/*width/height*/r.r.w,r.r.h,
				/*source*/0,0,
				priv_IFEcursor.saved);

		ifeapi->AddScreenUpdate(
			priv_IFEcursor.x+r.offset_x,priv_IFEcursor.y+r.offset_y,
			priv_IFEcursor.x+r.offset_x+r.r.w,priv_IFEcursor.y+r.offset_y+r.r.h);

		priv_IFEcursor.saved_valid = false;
	}
}

void priv_IFEDrawCursor(void) {
	if (priv_IFEcursor.bitmap == NULL) return;
	if (priv_IFEcursor.hide_draw > 0 || !priv_IFEcursor.show_state || priv_IFEcursor.host_replacement) return;

	const IFEBitmap::subrect &r = priv_IFEcursor.bitmap->get_subrect(priv_IFEcursor.bitmap_sr);

	if (!priv_IFEcursor.saved_valid) {
		if (!priv_IFEcursor.saved.alloc_storage(r.r.w,r.r.h))
			return;

		IFEBitBlt(	priv_IFEcursor.saved,
				/*dest*/0,0,
				/*width/height*/r.r.w,r.r.h,
				/*source*/priv_IFEcursor.x+r.offset_x,priv_IFEcursor.y+r.offset_y,
				*IFEscrbmp);

		if (priv_IFEcursor.bitmap->image_type == IFEBitmap::IMT_TRANSPARENT_MASK)
			IFETBitBlt(*IFEscrbmp,/*dest*/priv_IFEcursor.x+r.offset_x,priv_IFEcursor.y+r.offset_y,/*width/height*/r.r.w,r.r.h,/*source*/0,0,*priv_IFEcursor.bitmap);
		else
			IFEBitBlt(*IFEscrbmp,/*dest*/priv_IFEcursor.x+r.offset_x,priv_IFEcursor.y+r.offset_y,/*width/height*/r.r.w,r.r.h,/*source*/0,0,*priv_IFEcursor.bitmap);

		ifeapi->AddScreenUpdate(
			priv_IFEcursor.x+r.offset_x,priv_IFEcursor.y+r.offset_y,
			priv_IFEcursor.x+r.offset_x+r.r.w,priv_IFEcursor.y+r.offset_y+r.r.h);

		priv_IFEcursor.saved_valid = true;
	}
}

void IFEHideCursorDrawing(bool hide) {
	priv_IFEUndrawCursor();

	if (hide)
		priv_IFEcursor.hide_draw++;
	else
		priv_IFEcursor.hide_draw--;

	priv_IFEDrawCursor();
}

void IFEShowCursor(bool show) {
	if (priv_IFEcursor.show_state != show) {
		priv_IFEUndrawCursor();
		priv_IFEcursor.show_state = show;
		if (priv_IFEcursor.host_replacement) ifeapi->ShowHostStdCursor(show);
		priv_IFEDrawCursor();
	}
}

/* mouse cursor position is driven by platform mouse driver.
 * this function is used purely by the mouse tracking code and
 * does not affect the platform mouse driver's position, so
 * what's the point? make it private. */
void priv_IFEMoveCursor(int x,int y) {
	if (priv_IFEcursor.x != x || priv_IFEcursor.y != y) {
		priv_IFEUndrawCursor();
		priv_IFEcursor.x = x;
		priv_IFEcursor.y = y;
		priv_IFEDrawCursor();
	}
}

void IFEUpdateCursor(void) {
	IFEMouseStatus *ms = ifeapi->GetMouseStatus();
	priv_IFEMoveCursor(ms->x,ms->y);
}

void IFESetCursor(IFEBitmap* new_cursor,size_t sr) {
	if (new_cursor == priv_IFEcursor.bitmap && priv_IFEcursor.bitmap_sr == sr)
		return;

	priv_IFEUndrawCursor();

	if (new_cursor != NULL && sr < new_cursor->subrects_alloc) {
		unsigned int host_id = 0;

		priv_IFEcursor.bitmap_sr = sr;
		priv_IFEcursor.bitmap = new_cursor;

		if (new_cursor == &IFEcursor_arrow)
			host_id = IFEStdCursor_POINTER;
		else if (new_cursor == &IFEcursor_wait)
			host_id = IFEStdCursor_WAIT;

		if (ifeapi->SetHostStdCursor(host_id)) {
			priv_IFEcursor.host_replacement = true;
		}
		else {
			const IFEBitmap::subrect &r = priv_IFEcursor.bitmap->get_subrect(priv_IFEcursor.bitmap_sr);
			priv_IFEcursor.saved.alloc_storage(r.r.w,r.r.h);
			priv_IFEcursor.host_replacement = false;
		}
	}
	else {
		priv_IFEcursor.host_replacement = false;
		priv_IFEcursor.saved.free_storage();
		priv_IFEcursor.bitmap = NULL;
		priv_IFEcursor.bitmap_sr = 0;
	}

	priv_IFEDrawCursor();
}

void IFECheckEvent(void) {
	ifeapi->CheckEvents();
	IFEUpdateCursor();
	ifeapi->UpdateScreen();
}

void IFEWaitEvent(const int wait_ms) {
	ifeapi->WaitEvent(wait_ms);
	IFEUpdateCursor();
	ifeapi->UpdateScreen();
}

void IFENormalExit(void) {
#if defined(USE_DOSLIB)
	IFE_win95_tf_hang_check();
#endif

	ifeapi->ShutdownVideo();
	IFEscrbmp = NULL;
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

	if (!IFELoadPNG(IFEcursor_arrow,"st_arrow.png"))
		IFEFatalError("Failed to load arrow image");
	if (!IFELoadPNG(IFEcursor_wait,"st_wait.png"))
		IFEFatalError("Failed to load wait image");
	{/* by the way the hotspot is in the middle of the hourglass */
		IFEBitmap::subrect &r = IFEcursor_wait.get_subrect(0);
		r.offset_x = (short int)(-r.r.w / 2);
		r.offset_y = (short int)(-r.r.h / 2);
	}

	ifeapi->InitVideo();
	ifeapi->SetWindowTitle("Testing 123");

	IFESetCursor(&IFEcursor_arrow);
	IFEShowCursor(true);

	ifeapi->ResetTicks(ifeapi->GetTicks());
	while (ifeapi->GetTicks() < 1000) {
		if (ifeapi->UserWantsToQuit()) IFENormalExit();
		IFEWaitEvent(100);
	}

	IFEHideCursorDrawing(true);
	IFETestRGBPalette();
	IFETestRGBPalettePattern(IFEGetScreenBitmap());
	IFEHideCursorDrawing(false);
	ifeapi->UpdateFullScreen();
	ifeapi->ResetTicks(ifeapi->GetTicks());
	while (ifeapi->GetTicks() < 3000) {
		if (ifeapi->UserWantsToQuit()) IFENormalExit();
		IFEWaitEvent(100);
	}

	IFEShowCursor(false);

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

		ifeapi->SetWindowTitle("BitBlt BMP test");

		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/20,20,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
		IFEAddScreenUpdate(20,20,20+bmp.width,20+bmp.height);
		ifeapi->UpdateScreen();

		/* test clipping by letting the user mouse around with it */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEFillRect(IFEGetScreenBitmap(),px,py,px+bmp.width,py+bmp.height,0);
					IFEAddScreenUpdate(px,py,px+bmp.width,py+bmp.height);

					px = ms->x;
					py = ms->y;

					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
					IFELineRect(IFEGetScreenBitmap(),px,py,px+bmp.width,py+bmp.height,127);
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
				IFEWaitEvent(1);
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
					IFEFillRect(IFEGetScreenBitmap(),px,py,px+bmp.width,py+bmp.height,0);
					IFEAddScreenUpdate(px,py,px+bmp.width,py+bmp.height);

					px = ms->x;
					py = ms->y;

					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
					IFELineRect(IFEGetScreenBitmap(),px,py,px+bmp.width,py+bmp.height,127);
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
				IFEWaitEvent(1);
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
					IFEFillRect(IFEGetScreenBitmap(),px,py,px+bmp.width,py+bmp.height,0);
					IFEAddScreenUpdate(px,py,px+bmp.width,py+bmp.height);

					px = ms->x;
					py = ms->y;

					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
					IFELineRect(IFEGetScreenBitmap(),px,py,px+bmp.width,py+bmp.height,127);
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
				IFEWaitEvent(1);
			} while (1);
		}
	}

	{
		IFEBitmap combo_woo;
		IFEBitmap bmp,tbmp;
		IFEBitmap savebmp;

		IFEResetScissorRect();

		ifeapi->SetWindowTitle("BitBlt transparency test");

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
		if (tbmp.image_type != IFEBitmap::IMT_TRANSPARENT_MASK)
			IFEFatalError("PNG should have transparency");

		if (!savebmp.alloc_storage(tbmp.width,tbmp.height))
			IFEFatalError("SaveBMP alloc fail");

		/* blank screen to avoid palette flash */
		IFEBlankScreen(IFEGetScreenBitmap());
		ifeapi->UpdateFullScreen();
		ifeapi->SetPaletteColors(0,bmp.palette_size,bmp.palette);

		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/0,0,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/40,40,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
		IFETBitBlt(IFEGetScreenBitmap(),/*dest*/40,240,/*width,height*/tbmp.width/*image is 2x the width for mask*/,tbmp.height,/*source*/0,0,tbmp);
		ifeapi->UpdateFullScreen();

		IFESetCursor(&IFEcursor_wait);
		IFEShowCursor(true);

		ifeapi->ResetTicks(ifeapi->GetTicks());
		while (ifeapi->GetTicks() < 3000) {
			if (ifeapi->UserWantsToQuit()) IFENormalExit();
			IFEWaitEvent(1);
		}

		IFEShowCursor(false);

		/* test clipping by letting the user mouse around with it */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/tbmp.width,tbmp.height,/*source*/px,py,bmp);
					IFEAddScreenUpdate(px,py,px+tbmp.width,py+tbmp.height);
					px = ms->x;
					py = ms->y;
					IFETBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/tbmp.width/*image is 2x the width for mask*/,tbmp.height,/*source*/0,0,tbmp);
					IFEAddScreenUpdate(px,py,px+tbmp.width,py+tbmp.height);

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
				IFEWaitEvent(1);
			} while (1);
		}

		ifeapi->SetWindowTitle("BitBlt transparency test with save/restore");

		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/0,0,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/40,40,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
		IFETBitBlt(IFEGetScreenBitmap(),/*dest*/40,240,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
		ifeapi->UpdateFullScreen();

		/* test clipping by letting the user mouse around with it, this time using IFEBitBlt() to save and restore the background
		 * under the image. */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
					IFEAddScreenUpdate(px,py,px+tbmp.width,py+tbmp.height);
					px = ms->x;
					py = ms->y;
					IFEBitBlt(savebmp,/*dest*/0,0,/*width,height*/savebmp.width,savebmp.height,/*source*/px,py,IFEGetScreenBitmap());
					IFETBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
					IFEAddScreenUpdate(px,py,px+tbmp.width,py+tbmp.height);

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
				IFEWaitEvent(1);
			} while (1);
		}

		ifeapi->SetWindowTitle("BitBlt transparency test with save/restore composite blt");

		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/0,0,/*width,height*/bmp.width,bmp.height,/*source*/0,0,bmp);
		IFEBitBlt(IFEGetScreenBitmap(),/*dest*/40,40,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
		IFETBitBlt(IFEGetScreenBitmap(),/*dest*/40,240,/*width,height*/tbmp.width,tbmp.height,/*source*/0,0,tbmp);
		ifeapi->UpdateFullScreen();

		IFEShowCursor(true);

		if (!combo_woo.alloc_storage(tbmp.width*2,tbmp.height*2))
			IFEFatalError("combo woo alloc fail");
		if (!savebmp.alloc_storage(combo_woo.width,combo_woo.height))
			IFEFatalError("savebmp alloc fail");

		IFEFillRect(combo_woo,0,0,combo_woo.width,combo_woo.height,0);
		IFEBitBlt(combo_woo,0,             0,             tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,tbmp.width,    0,             tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,0,             tbmp.height,   tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,tbmp.width,    tbmp.height,   tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,tbmp.width/2,  tbmp.height/2, tbmp.width,tbmp.height,0,0,tbmp);

		IFEShowCursor(false);

		/* test clipping by letting the user mouse around with it, this time using IFEBitBlt() to save and restore the background
		 * under the image. */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
					IFEAddScreenUpdate(px,py,px+combo_woo.width,py+combo_woo.height);
					px = ms->x;
					py = ms->y;
					IFEBitBlt(savebmp,/*dest*/0,0,/*width,height*/savebmp.width,savebmp.height,/*source*/px,py,IFEGetScreenBitmap());
					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/combo_woo.width,combo_woo.height,/*source*/0,0,combo_woo);
					IFEAddScreenUpdate(px,py,px+combo_woo.width,py+combo_woo.height);

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
				IFEWaitEvent(1);
			} while (1);

			IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
		}

		IFEShowCursor(true);

		if (!combo_woo.alloc_storage(tbmp.width*2,tbmp.height*2,IFEBitmap::IMT_TRANSPARENT_MASK))
			IFEFatalError("combo woo alloc fail");
		if (!savebmp.alloc_storage(combo_woo.width,combo_woo.height))
			IFEFatalError("savebmp alloc fail");

		IFEFillRect(combo_woo,0,0,combo_woo.width,combo_woo.height,0);
		IFEBitBlt(combo_woo,0,             0,             tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,tbmp.width,    0,             tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,0,             tbmp.height,   tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,tbmp.width,    tbmp.height,   tbmp.width,tbmp.height,0,0,tbmp);
		IFEBitBlt(combo_woo,tbmp.width/2,  tbmp.height/2, tbmp.width,tbmp.height,0,0,tbmp);

		IFEShowCursor(false);

		/* test clipping by letting the user mouse around with it, this time using IFEBitBlt() to save and restore the background
		 * under the image. */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
					IFEAddScreenUpdate(px,py,px+combo_woo.width,py+combo_woo.height);
					px = ms->x;
					py = ms->y;
					IFEBitBlt(savebmp,/*dest*/0,0,/*width,height*/savebmp.width,savebmp.height,/*source*/px,py,IFEGetScreenBitmap());
					IFETBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/combo_woo.width,combo_woo.height,/*source*/0,0,combo_woo);
					IFEAddScreenUpdate(px,py,px+combo_woo.width,py+combo_woo.height);

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
				IFEWaitEvent(1);
			} while (1);

			IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
		}

		IFEShowCursor(true);

		if (!combo_woo.alloc_storage(tbmp.width*2,tbmp.height*2,IFEBitmap::IMT_TRANSPARENT_MASK))
			IFEFatalError("combo woo alloc fail");
		if (!savebmp.alloc_storage(combo_woo.width,combo_woo.height))
			IFEFatalError("savebmp alloc fail");

		IFETFillRect(combo_woo,0,0,combo_woo.width,combo_woo.height); /* fill image with transparent */
		IFETBitBlt(combo_woo,0,             0,             tbmp.width,tbmp.height,0,0,tbmp);
		IFETBitBlt(combo_woo,tbmp.width,    0,             tbmp.width,tbmp.height,0,0,tbmp);
		IFETBitBlt(combo_woo,0,             tbmp.height,   tbmp.width,tbmp.height,0,0,tbmp);
		IFETBitBlt(combo_woo,tbmp.width,    tbmp.height,   tbmp.width,tbmp.height,0,0,tbmp);
		IFETBitBlt(combo_woo,tbmp.width/2,  tbmp.height/2, tbmp.width,tbmp.height,0,0,tbmp);

		IFEShowCursor(false);

		/* test clipping by letting the user mouse around with it, this time using IFEBitBlt() to save and restore the background
		 * under the image. */
		{
			int px = -999,py = -999;
			int lx = -999,ly = -999;

			do {
				IFEMouseStatus *ms = ifeapi->GetMouseStatus();

				if (px != ms->x || py != ms->y) {
					IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
					IFEAddScreenUpdate(px,py,px+combo_woo.width,py+combo_woo.height);
					px = ms->x;
					py = ms->y;
					IFEBitBlt(savebmp,/*dest*/0,0,/*width,height*/savebmp.width,savebmp.height,/*source*/px,py,IFEGetScreenBitmap());
					IFETBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/combo_woo.width,combo_woo.height,/*source*/0,0,combo_woo);
					IFEAddScreenUpdate(px,py,px+combo_woo.width,py+combo_woo.height);

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
				IFEWaitEvent(1);
			} while (1);

			IFEBitBlt(IFEGetScreenBitmap(),/*dest*/px,py,/*width,height*/savebmp.width,savebmp.height,/*source*/0,0,savebmp);
		}
	}

	ifeapi->SetWindowTitle("More");

	IFESetCursor(&IFEcursor_arrow);

	/* blank screen to avoid palette flash */
	IFEBlankScreen(IFEGetScreenBitmap());
	ifeapi->UpdateFullScreen();

	IFETestRGBPalette();

	IFETestRGBPalettePattern2(IFEGetScreenBitmap());
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
			IFEWaitEvent(1);
		}
	}
	IFETestRGBPalettePattern(IFEGetScreenBitmap());
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
			IFEWaitEvent(1);
		}
	}

	ifeapi->UpdateFullScreen();
	IFEShowCursor(true);

	while (ifeapi->GetTicks() < 6000) {
		if (ifeapi->UserWantsToQuit()) IFENormalExit();
		IFEWaitEvent(100);
	}

	IFEShowCursor(false);

	IFENormalExit();
	return 0;
}

