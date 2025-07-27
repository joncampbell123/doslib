
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
#  define BITMAP_FARPTR_RMNORM
# endif
#endif

struct BMPFILEIMAGE {
#ifdef BITMAP_SLICES
# error not yet supported
#else
	unsigned char*		bitmap;
#endif
	unsigned int		width,height,stride,bpp;
#ifdef BITMAP_FARPTR_RMNORM
# error not yet supported
#endif
};

static struct BMPFILEIMAGE membmp;

/* For our sanity's sake we read the bitmap bottom-up, store in memory top-down, write to disk bottom-up. */
/* NTS: Future plans: Compile as 16-bit real mode DOS, and this function will use FAR pointer normalization to return bitmap scanlines properly.
 * NTS: Future plans: Compile as 16-bit Windows, and this program will allocate the bitmap in slices and this function will map to slice and scanline. */
unsigned char *bitmap_row(const struct BMPFILEIMAGE *bfi,unsigned int y) {
	if (bfi->bitmap != NULL || y >= bfi->height)
		return bfi->bitmap + (bfi->stride * y);

	return NULL;
}

unsigned int bitmap_stride_from_bpp_and_w(unsigned int bpp,unsigned int w) {
	return (((w * bpp) + 31u) & (~31u)) >> 3u;
}

unsigned char bitmap_mkbf8(uint32_t w,const uint8_t fs,const uint8_t fw) {
	if (fw != 0u) {
		w >>= fs;
		w &= (1u << fw) - 1u;
		if (fw > 8u) w >>= (uint32_t)(fw - 8u); /* truncate to 8 bits if larger */
		if (fw < 8u) w = (w * 255u) / ((1u << fw) - 1u);
		return (unsigned char)w;
	}

	return 0;
}

void bitmap_memcpy32to24(unsigned char *d24,const unsigned char *s32raw,unsigned int w,const struct BMPFILEREAD *bfr) {
	const uint32_t *s32 = (const uint32_t*)s32raw;

	while (w-- > 0) {
		const uint32_t w = *s32++;
		*d24++ = bitmap_mkbf8(w,bfr->blue_shift, bfr->blue_width);
		*d24++ = bitmap_mkbf8(w,bfr->green_shift,bfr->green_width);
		*d24++ = bitmap_mkbf8(w,bfr->red_shift,  bfr->red_width);
	}
}

int main(int argc,char **argv) {
	if (argc < 3)
		return 1;

	memset(&membmp,0,sizeof(membmp));

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

		membmp.bpp = 24;
		membmp.width = bfr->width;
		membmp.height = bfr->height;
		membmp.stride = bitmap_stride_from_bpp_and_w(membmp.bpp,membmp.width);

		/* NTS: Careful, malloc() on 16-bit DOS might only have a 16-bit param! You'll need
		 *      to use _fmalloc() and pass in size as paragraphs! */
		/* NTS: 16-bit Windows, this code will need to make multiple allocations of bitmap slices
		 *      less than 64KB, perhaps using LocalAlloc() or GlobalAlloc() */
		membmp.bitmap = malloc(membmp.stride * membmp.height);

		if (!membmp.bitmap) {
			fprintf(stderr,"Failed to allocate memory\n");
			return 1;
		}

		while (read_bmp_line(bfr) == 0) {
			unsigned char *dest = bitmap_row(&membmp,(unsigned int)bfr->current_line);
			assert(dest != NULL);

			if (bfr->bpp == 32)
				bitmap_memcpy32to24(dest,bfr->scanline,membmp.width,bfr);
			else
				memcpy(dest,bfr->scanline,membmp.stride);
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

		wstride = bitmap_stride_from_bpp_and_w(membmp.bpp,membmp.width);

		fd = open(argv[2],O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644);
		if (fd < 0) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		memset(&bfh,0,sizeof(bfh));
		bfh.bfType = 0x4D42; /* 'BM' */
		bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
		bfh.bfSize = ((unsigned long)membmp.height * (unsigned long)wstride) + bfh.bfOffBits;
		if ((unsigned int)write(fd,&bfh,sizeof(bfh)) != sizeof(bfh)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		memset(&bih,0,sizeof(bih));
		bih.biPlanes = 1;
		bih.biSize = sizeof(bih);
		bih.biWidth = membmp.width;
		bih.biHeight = membmp.height;
		bih.biBitCount = membmp.bpp;
		bih.biSizeImage = (unsigned long)membmp.height * (unsigned long)wstride;
		if ((unsigned int)write(fd,&bih,sizeof(bih)) != sizeof(bih)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		if (lseek(fd,0,SEEK_CUR) != bfh.bfOffBits) {
			fprintf(stderr,"Offset error\n");
			return 1;
		}

		for (y=0;y < membmp.height;y++) {
			s = bitmap_row(&membmp,membmp.height - 1u - y);/* normal bitmaps are bottom up */
			assert(s != NULL);

			if ((unsigned int)write(fd,s,wstride) != wstride) {
				fprintf(stderr,"Scanline write err\n");
				return 1;
			}
		}

		close(fd);
	}

	/* free bitmap */
	free(membmp.bitmap);
	return 0;
}

