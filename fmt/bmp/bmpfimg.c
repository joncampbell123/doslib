
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

#if defined(ENABLE_BMPFILEIMAGE)
struct BMPFILEIMAGE *bmpfileimage_alloc(void) {
	struct BMPFILEIMAGE *b = (struct BMPFILEIMAGE*)malloc(sizeof(struct BMPFILEIMAGE));
	if (b) memset(b,0,sizeof(*b));
	return b;
}

void bmpfileimage_free_image(struct BMPFILEIMAGE *b) {
#if defined(BMPFILEIMAGE_GMEM_ARRAY) && TARGET_MSDOS == 16
	if (b->strips) {
		{
			unsigned i;
			for (i=0;i < b->strip_count;i++) {
				if (b->strips[i].ghandle) {
					if (b->strips[i].ptr) GlobalUnlock(b->strips[i].ghandle);
					b->strips[i].ptr = NULL;

					GlobalFree(b->strips[i].ghandle);
					b->strips[i].ghandle = (HGLOBAL)NULL;
				}
			}
		}

		free(b->strips);
		b->strips = NULL;
	}
#elif defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
	if (b->bitmap_seg) {
		_dos_freemem(b->bitmap_seg);
		b->bitmap_seg = 0;
	}
#else
	if (b->bitmap) {
		free(b->bitmap);
		b->bitmap = NULL;
	}
#endif
	b->stride = 0;
}

void bmpfileimage_free(struct BMPFILEIMAGE **b) {
	if (*b) {
		bmpfileimage_free_image(*b);
		free(*b); *b = NULL;
	}
}

int bmpfileimage_alloc_image(struct BMPFILEIMAGE *membmp) {
#if defined(BMPFILEIMAGE_GMEM_ARRAY) && TARGET_MSDOS == 16
	if (membmp->strips)
		return -1;
#elif defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
	if (membmp->bitmap_seg)
		return -1;
#else
	if (membmp->bitmap)
		return -1;
#endif

	if (membmp->stride == 0)
		membmp->stride = bitmap_stride_from_bpp_and_w(membmp->bpp,membmp->width);
	if (membmp->stride == 0 || membmp->stride > 32768u || membmp->height == 0)
		return -1;

#if defined(BMPFILEIMAGE_GMEM_ARRAY) && TARGET_MSDOS == 16
	membmp->strip_height = 0xFFF0u / membmp->stride;
	if (!membmp->strip_height)
		return -1;

	membmp->strip_count = membmp->height / membmp->strip_height;
	if (!membmp->strip_count)
		return -1;

	membmp->last_strip_height = membmp->height % membmp->strip_height;
	if (membmp->last_strip_height)
		membmp->strip_count++;

	membmp->strips = malloc(membmp->strip_count * sizeof(struct BMPFILEIMAGE_WIN16_HG));
	if (!membmp->strips)
		return -1;
	memset(membmp->strips,0,membmp->strip_count * sizeof(struct BMPFILEIMAGE_WIN16_HG));

	{
		unsigned i;
		HGLOBAL gl;

		for (i=0;i < membmp->strip_count;i++) {
			if (membmp->last_strip_height && (i+1u) == membmp->strip_count)
				gl = GlobalAlloc(GHND,membmp->last_strip_height * membmp->stride);
			else
				gl = GlobalAlloc(GHND,membmp->strip_height * membmp->stride);

			if (gl == (HGLOBAL)NULL)
				return -1;

			membmp->strips[i].ghandle = gl;
		}
	}
#elif defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
	{
		const unsigned long sz = (unsigned long)membmp->stride * (unsigned long)membmp->height; /* in bytes */
		unsigned s;

		/* Real-mode MS-DOS usually has 640KB of RAM and cannot address more than 1MB of RAM, cap at 960KB */
		if (sz >= (960ul * 1024ul))
			return -1;

		if (_dos_allocmem((unsigned)((sz + 0xFu) >> 4ul)/*bytes->paragraphs*/,&s))
			return -1;

		membmp->bitmap_seg = s;
	}
#else
	membmp->bitmap = malloc(membmp->stride * membmp->height);
	if (!membmp->bitmap)
		return -1;
#endif

	return 0;
}

unsigned char BMPFAR *bmpfileimage_row(const struct BMPFILEIMAGE *bfi,unsigned int y) {
#if defined(BMPFILEIMAGE_GMEM_ARRAY) && TARGET_MSDOS == 16
	if (bfi->strips && bfi->strip_count && y < bfi->height) {
		const unsigned int strip = y / bfi->strip_height;
		const unsigned int sy = y % bfi->strip_height;

		/* if the last strip, make sure the strip y doesn't exceed it */
		if ((strip+1u) == bfi->strip_count && bfi->last_strip_height) {
			if (sy >= bfi->last_strip_height)
				return NULL;
		}

		if (!bfi->strips[strip].ghandle)
			return NULL;

		if (!bfi->strips[strip].ptr) {
			if ((bfi->strips[strip].ptr=GlobalLock(bfi->strips[strip].ghandle)) == NULL)
				return NULL;
		}

		/* GlobalLock shouldn't return a pointer that crosses segment boundaries for blocks < 64KB,
		 * which is a problem if it does because FAR pointer math in Open Watcom C/C++ really only
		 * affects the offset field anyway and unexpected corruption would occur.
		 *
		 * Each memory block is allocated such that it is < 64KB and enough to hold exactly
		 * strip_height * stride bytes of memory so that the calling program never has to deal with
		 * FAR pointer math. Therefore strip_height * stride should never exceed 64KB - 1 and should
		 * never require more than a 16-bit unsigned int to handle. */
		if (((unsigned long)FP_OFF(bfi->strips[strip].ptr) + (unsigned long)bfi->stride) > 0xFFFFul)
			return NULL;

		return (unsigned char FAR*)bfi->strips[strip].ptr + (sy * bfi->stride);
	}
#elif defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
	if (bfi->bitmap_seg && y < bfi->height) {
		const unsigned long ofs = (unsigned long)bfi->stride * (unsigned long)y;
		const unsigned s = bfi->bitmap_seg + (unsigned)(ofs >> 4ul);
		const unsigned o = (unsigned)(ofs & 0xFul);
		return MK_FP(s,o);
	}
#else
	if (bfi->bitmap != NULL && y < bfi->height)
		return bfi->bitmap + (bfi->stride * y);
#endif

	return NULL;
}
#endif

