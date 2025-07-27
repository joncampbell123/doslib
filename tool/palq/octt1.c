
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

#if TARGET_MSDOS == 16
# if defined(TARGET_WINDOWS)
/* 16-bit Windows: We can hold a bitmap larger than 64KB, and even point at rows, we just have to store it in slices
 * less than 64KB to keep our sanity */
#  define BITMAP_SLICES
#  define MAX_BITMAP_SLICE (0xFF00ul)
# else
/* 16-bit MS-DOS: We can do some real mode FAR pointer normalization to keep our sanity, no scanline we handle will exceed 64KB. */
#  define REALMODE_FARPTR_NORM
# endif
#endif

#ifdef BITMAP_SLICES
# error not yet supported
#else
char *bitmap = NULL;
#endif
unsigned int bitmap_width = 0,bitmap_height = 0,bitmap_stride = 0;

#ifdef REALMODE_FARPTR_NORM
# error not yet supported
#endif

/* For our sanity's sake we read the bitmap bottom-up, store in memory top-down, write to disk bottom-up. */
/* NTS: Future plans: Compile as 16-bit real mode DOS, and this function will use FAR pointer normalization to return bitmap scanlines properly.
 * NTS: Future plans: Compile as 16-bit Windows, and this program will allocate the bitmap in slices and this function will map to slice and scanline. */
char *bitmap_row(unsigned int y) {
	if (bitmap != NULL || y >= bitmap_height)
		return bitmap + (bitmap_stride * y);

	return NULL;
}

int main(int argc,char **argv) {
	struct BMPFILEREAD *bfr;

	bfr = open_bmp(argv[1]);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (!(bfr->bpp == 24 || bfr->bpp == 32)) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	bitmap_width = bfr->width;
	bitmap_height = bfr->height;
	bitmap_stride = bfr->stride;

	/* NTS: Careful, malloc() on 16-bit DOS might only have a 16-bit param! You'll need
	 *      to use _fmalloc() and pass in size as paragraphs! */
	/* NTS: 16-bit Windows, this code will need to make multiple allocations of bitmap slices
	 *      less than 64KB, perhaps using LocalAlloc() or GlobalAlloc() */
	bitmap = malloc(bitmap_stride * bitmap_height);

	if (!bitmap) {
		fprintf(stderr,"Failed to allocate memory\n");
		return 1;
	}

	while (read_bmp_line(bfr) == 0) {
		char *dest = bitmap_row((unsigned int)bfr->current_line);
		assert(dest != NULL);
		memcpy(dest,bfr->scanline,bitmap_stride);
	}

	/* done reading */
	close_bmp(&bfr);

	/* free bitmap */
	free(bitmap);
	return 0;
}

