
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
#if defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
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
#if defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
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

#if defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
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
#if defined(BMPFILEIMAGE_SEGMENT_BASE) && TARGET_MSDOS == 16
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

