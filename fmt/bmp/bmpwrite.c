
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

struct BMPFILEWRITE *create_write_bmp(void) {
	struct BMPFILEWRITE *b = (struct BMPFILEWRITE*)malloc(sizeof(struct BMPFILEWRITE));
	if (b) {
		memset(b,0,sizeof(*b));
		b->fd = -1;
		return b;
	}

	return NULL;
}

int open_write_bmp(struct BMPFILEWRITE *bmp,const char *path) {
	if (bmp->fd >= 0 || path == NULL)
		return -1;
	if (bmp->width == 0 || bmp->height == 0 || bmp->bpp == 0 || bmp->width > 4096 || bmp->height > 4096)
		return -1;
	if (!(bmp->bpp == 15 || bmp->bpp == 16 || bmp->bpp == 24 || bmp->bpp == 32))
		return -1;

	/* bottom up */
	bmp->current_line = bmp->height - 1;
	bmp->current_line_add = -1;

	bmp->stride = bitmap_stride_from_bpp_and_w(bmp->bpp,bmp->width);

	bmp->fd = open(path,O_CREAT|O_TRUNC|O_BINARY|O_WRONLY,0644);
	if (bmp->fd < 0)
		return -1;

	{
		struct winBITMAPFILEHEADER bfh;
		struct winBITMAPINFOHEADER bih;

		memset(&bfh,0,sizeof(bfh));
		bfh.bfType = 0x4D42; /* 'BM' */
		bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
		bfh.bfSize = ((unsigned long)bmp->height * (unsigned long)bmp->stride) + bfh.bfOffBits;
		if ((unsigned int)write(bmp->fd,&bfh,sizeof(bfh)) != sizeof(bfh))
			return 1;

		memset(&bih,0,sizeof(bih));
		bih.biPlanes = 1;
		bih.biSize = sizeof(bih);
		bih.biWidth = bmp->width;
		bih.biHeight = bmp->height;
		bih.biBitCount = bmp->bpp;
		bih.biSizeImage = (unsigned long)bmp->height * (unsigned long)bmp->stride;
		if ((unsigned int)write(bmp->fd,&bih,sizeof(bih)) != sizeof(bih))
			return 1;

		if (lseek(bmp->fd,0,SEEK_CUR) != bfh.bfOffBits)
			return 1;
	}

	return 0;
}

int write_bmp_line(struct BMPFILEWRITE *bmp,const unsigned char BMPFAR *row,const unsigned int len) {
	if (bmp->fd < 0 || row == NULL)
		return -1;
	if (len < bmp->stride)
		return -1;
	if (bmp->current_line < 0 || (unsigned int)bmp->current_line >= bmp->height)
		return -1;

	if ((unsigned int)write(bmp->fd,row,bmp->stride) != bmp->stride)
		return -1;

	bmp->current_line += bmp->current_line_add;
	return 0;
}

void do_close_write_bmp_file(struct BMPFILEWRITE *bmp) {
	if (bmp->fd >= 0) {
		close(bmp->fd);
		bmp->fd = -1;
	}
}

void do_close_write_bmp(struct BMPFILEWRITE *bmp) {
	do_close_write_bmp_file(bmp);
}

void close_write_bmp(struct BMPFILEWRITE **bmp) {
	if (*bmp) {
		do_close_write_bmp(*bmp);
		free(*bmp);
		*bmp = NULL;
	}
}

