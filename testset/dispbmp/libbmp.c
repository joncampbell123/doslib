
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <i86.h>

#include "libbmp.h"

struct BMPFILEREAD *open_bmp(const char *path) {
	struct BMPFILEREAD *bmp = (struct BMPFILEREAD*)calloc(1,sizeof(struct BMPFILEREAD));
	unsigned char *temp;

	if (bmp == NULL) {
		errno = ENOMEM;
		return NULL;
	}

	temp = malloc(256); /* for temporary loading only */
	if (temp == NULL) {
		errno = ENOMEM;
		free(bmp);
		return NULL;
	}

	bmp->fd = open(path,O_RDONLY | O_BINARY); /* sets errno */
	if (bmp->fd < 0) goto fail_fileopen;

	/* file header */
	if (lseek(bmp->fd,0,SEEK_SET) != 0) goto fail;
	if (read(bmp->fd,temp,14) != 14) goto fail;
	{
		struct winBITMAPFILEHEADER *fh = (struct winBITMAPFILEHEADER*)temp;
		if (fh->bfType != 0x4D42) goto fail;
		bmp->dib_offset = fh->bfOffBits;
	}

	/* then the BITMAPINFOHEADER */
	if (read(bmp->fd,temp,4) != 4) goto fail;
	{
		struct winBITMAPINFOHEADER *bi = (struct winBITMAPINFOHEADER*)temp;
		/* we just read the first 4 bytes, the biSize field */
		/* we don't support the old BITMAPCOREINFO that OS/2 uses, sorry */
		/* nothing is defined that is so large to need more than 256 bytes */
		if (bi->biSize < 40 || bi->biSize > 256) goto fail;
		/* now read the rest of it */
		if (read(bmp->fd,temp+4,bi->biSize-4) != (bi->biSize-4)) goto fail;

		if (bi->biWidth <= 0 || bi->biHeight == 0 || bi->biWidth > 0xFFFF || bi->biHeight > 0xFFFF || bi->biBitCount == 0 || bi->biPlanes > 1) goto fail;

		/* bitmaps are upside down by default, Windows 95 and later make the field signed and negative means topdown */
		if (bi->biHeight < 0) {
			bmp->current_line = -1;
			bmp->current_line_add = 1;
		}
		else {
			bmp->current_line = (int)bi->biHeight;
			bmp->current_line_add = -1;
		}

		bmp->width = (unsigned int)(bi->biWidth);
		bmp->height = (unsigned int)labs(bi->biHeight);
		bmp->bpp = bi->biBitCount;

		if (bi->biCompression == 0) {
			bmp->stride = (((bmp->width * bmp->bpp) + 31u) & (~31u)) >> 3u;
			if (bmp->stride == 0) goto fail;
			bmp->scanline_size = bmp->stride;
			bmp->scanline = malloc(bmp->scanline_size + 8u);
			if (bmp->scanline == NULL) goto fail;
			bmp->size = (unsigned long)bmp->stride * (unsigned long)bmp->height;

			if (bmp->bpp <= 8) {
				bmp->colors = 1u << bmp->bpp;
				if (bi->biClrUsed != 0 && bmp->colors > bi->biClrUsed) bmp->colors = bi->biClrUsed;
				if (bmp->colors == 0) goto fail;

				bmp->palette = (struct BMPPALENTRY*)calloc(bmp->colors,sizeof(struct BMPPALENTRY));
				if (bmp->palette == NULL) goto fail;

				/* palette follows struct */
				if (read(bmp->fd,bmp->palette,bmp->colors * sizeof(struct BMPPALENTRY)) != (bmp->colors * sizeof(struct BMPPALENTRY))) goto fail;
			}
		}
		else {
			goto fail;
		}
	}

	/* then prepare for reading the bitmap */
	if (lseek(bmp->fd,bmp->dib_offset,SEEK_SET) != bmp->dib_offset) goto fail;

	free(temp);
	return bmp;
fail:
	close(bmp->fd);
fail_fileopen:
	if (bmp->scanline) free(bmp->scanline);
	if (bmp->palette) free(bmp->palette);
	free(temp);
	free(bmp);
	return NULL;
}

static void do_close_bmp(struct BMPFILEREAD *bmp) {
	if (bmp->palette) free(bmp->palette);
	bmp->palette = NULL;
	if (bmp->scanline) free(bmp->scanline);
	bmp->scanline = NULL;
	if (bmp->fd >= 0) close(bmp->fd);
	bmp->fd = -1;
	free(bmp);
}

void close_bmp(struct BMPFILEREAD **bmp) {
	if (bmp && *bmp) {
		do_close_bmp(*bmp);
		*bmp = NULL;
	}
}

int read_bmp_line(struct BMPFILEREAD *bmp) {
	if (bmp == NULL) return -1;
	if (bmp->fd < 0) return -1;
	if (bmp->scanline == NULL || bmp->scanline_size == 0) return -1;
	if (bmp->stride == 0 || bmp->stride > bmp->scanline_size) return -1;

	bmp->current_line += bmp->current_line_add;
	if (bmp->current_line < 0 || (unsigned int)bmp->current_line >= bmp->height) return -1;

	if (read(bmp->fd,bmp->scanline,bmp->stride) != bmp->stride) return -1;

	return 0;
}

