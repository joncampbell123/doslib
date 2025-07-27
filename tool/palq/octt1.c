
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

#ifndef O_BINARY
# define O_BINARY 0
#endif

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
unsigned char *bitmap = NULL;
#endif
unsigned int bitmap_width = 0,bitmap_height = 0,bitmap_stride = 0;

#ifdef REALMODE_FARPTR_NORM
# error not yet supported
#endif

/* For our sanity's sake we read the bitmap bottom-up, store in memory top-down, write to disk bottom-up. */
/* NTS: Future plans: Compile as 16-bit real mode DOS, and this function will use FAR pointer normalization to return bitmap scanlines properly.
 * NTS: Future plans: Compile as 16-bit Windows, and this program will allocate the bitmap in slices and this function will map to slice and scanline. */
unsigned char *bitmap_row(unsigned int y) {
	if (bitmap != NULL || y >= bitmap_height)
		return bitmap + (bitmap_stride * y);

	return NULL;
}

void memcpy32to24(unsigned char *d24,const unsigned char *d32,unsigned int w) {
	while (w-- > 0) {
		*d24++ = *d32++; // B
		*d24++ = *d32++; // G
		*d24++ = *d32++; // R

		d32++; // skip A
	}
}

int main(int argc,char **argv) {
	if (argc < 3)
		return 1;

	{
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
			unsigned char *dest = bitmap_row((unsigned int)bfr->current_line);
			assert(dest != NULL);

			if (bfr->bpp == 32)
				memcpy32to24(dest,bfr->scanline,bitmap_width);
			else
				memcpy(dest,bfr->scanline,bitmap_stride);
		}

		/* done reading */
		close_bmp(&bfr);
	}

	/* write it out */
	{
		struct winBITMAPFILEHEADER bfh;
		struct winBITMAPINFOHEADER bih;
		unsigned long wstride;
		unsigned char *s;
		unsigned int y;
		int fd;

		wstride = (((bitmap_width * 24) + 31u) & (~31u)) >> 3u;

		fd = open(argv[2],O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644);
		if (fd < 0) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		memset(&bfh,0,sizeof(bfh));
		bfh.bfType = 0x4D42; /* 'BM' */
		bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
		bfh.bfSize = ((unsigned long)bitmap_height * (unsigned long)wstride) + bfh.bfOffBits;
		if ((unsigned int)write(fd,&bfh,sizeof(bfh)) != sizeof(bfh)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		memset(&bih,0,sizeof(bih));
		bih.biSize = sizeof(bih);
		bih.biWidth = bitmap_width;
		bih.biHeight = bitmap_height;
		bih.biPlanes = 1;
		bih.biBitCount = 24;
		bih.biSizeImage = (unsigned long)bitmap_height * (unsigned long)wstride;
		if ((unsigned int)write(fd,&bih,sizeof(bih)) != sizeof(bih)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		if (lseek(fd,0,SEEK_CUR) != bfh.bfOffBits) {
			fprintf(stderr,"Offset error\n");
			return 1;
		}

		for (y=0;y < bitmap_height;y++) {
			s = bitmap_row(bitmap_height - 1u - y);/* normal bitmaps are bottom up */
			assert(s != NULL);

			if ((unsigned int)write(fd,s,wstride) != wstride) {
				fprintf(stderr,"Scanline write err\n");
				return 1;
			}
		}

		close(fd);
	}

	/* free bitmap */
	free(bitmap);
	return 0;
}

