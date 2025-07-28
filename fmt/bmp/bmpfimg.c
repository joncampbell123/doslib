
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
	if (b->bitmap) {
		free(b->bitmap);
		b->bitmap = NULL;
	}
	b->stride = 0;
}

void bmpfileimage_free(struct BMPFILEIMAGE **b) {
	if (*b) {
		bmpfileimage_free_image(*b);
		free(*b); *b = NULL;
	}
}

int bmpfileimage_alloc_image(struct BMPFILEIMAGE *membmp) {
	if (membmp->bitmap)
		return -1;

	if (membmp->stride == 0)
		membmp->stride = bitmap_stride_from_bpp_and_w(membmp->bpp,membmp->width);
	if (membmp->stride == 0 || membmp->stride > 32768u)
		return -1;

	/* NTS: Careful, malloc() on 16-bit DOS might only have a 16-bit param! You'll need
	 *      to use _fmalloc() and pass in size as paragraphs! */
	/* NTS: 16-bit Windows, this code will need to make multiple allocations of bitmap slices
	 *      less than 64KB, perhaps using LocalAlloc() or GlobalAlloc() */
	membmp->bitmap = malloc(membmp->stride * membmp->height);
	if (!membmp->bitmap)
		return -1;

	return 0;
}

/* For our sanity's sake we read the bitmap bottom-up, store in memory top-down, write to disk bottom-up. */
/* NTS: Future plans: Compile as 16-bit real mode DOS, and this function will use FAR pointer normalization to return bitmap scanlines properly.
 * NTS: Future plans: Compile as 16-bit Windows, and this program will allocate the bitmap in slices and this function will map to slice and scanline. */
unsigned char BMPFAR *bmpfileimage_row(const struct BMPFILEIMAGE *bfi,unsigned int y) {
	if (bfi->bitmap != NULL || y >= bfi->height)
		return bfi->bitmap + (bfi->stride * y);

	return NULL;
}
#endif

