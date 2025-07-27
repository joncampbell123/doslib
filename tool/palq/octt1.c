
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

int main(int argc,char **argv) {
	struct BMPFILEIMAGE *membmp = NULL;

	if (argc < 3)
		return 1;

	membmp = bmpfileimage_alloc();
	if (!membmp)
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

		membmp->bpp = 24;
		membmp->width = bfr->width;
		membmp->height = bfr->height;

		if (!bmpfileimage_alloc_image(membmp)) {
			fprintf(stderr,"Failed to allocate memory\n");
			return 1;
		}

		while (read_bmp_line(bfr) == 0) {
			unsigned char *dest = bmpfileimage_row(membmp,(unsigned int)bfr->current_line);
			assert(dest != NULL);

			if (bfr->bpp == 32)
				bitmap_memcpy32to24(dest,bfr->scanline,membmp->width,bfr);
			else
				memcpy(dest,bfr->scanline,membmp->stride);
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

		wstride = bitmap_stride_from_bpp_and_w(membmp->bpp,membmp->width);

		fd = open(argv[2],O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644);
		if (fd < 0) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		memset(&bfh,0,sizeof(bfh));
		bfh.bfType = 0x4D42; /* 'BM' */
		bfh.bfOffBits = sizeof(bfh) + sizeof(bih);
		bfh.bfSize = ((unsigned long)membmp->height * (unsigned long)wstride) + bfh.bfOffBits;
		if ((unsigned int)write(fd,&bfh,sizeof(bfh)) != sizeof(bfh)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		memset(&bih,0,sizeof(bih));
		bih.biPlanes = 1;
		bih.biSize = sizeof(bih);
		bih.biWidth = membmp->width;
		bih.biHeight = membmp->height;
		bih.biBitCount = membmp->bpp;
		bih.biSizeImage = (unsigned long)membmp->height * (unsigned long)wstride;
		if ((unsigned int)write(fd,&bih,sizeof(bih)) != sizeof(bih)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		if (lseek(fd,0,SEEK_CUR) != bfh.bfOffBits) {
			fprintf(stderr,"Offset error\n");
			return 1;
		}

		for (y=0;y < membmp->height;y++) {
			s = bmpfileimage_row(membmp,membmp->height - 1u - y);/* normal bitmaps are bottom up */
			assert(s != NULL);

			if ((unsigned int)write(fd,s,wstride) != wstride) {
				fprintf(stderr,"Scanline write err\n");
				return 1;
			}
		}

		close(fd);
	}

	/* free bitmap */
	bmpfileimage_free(&membmp);
	return 0;
}

