
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

int main(int argc,char **argv) {
#if defined(ENABLE_BMPFILEIMAGE)
	struct BMPFILEIMAGE *membmp = NULL;
	const char *sfname,*dfname;

	if (argc < 2)
		return 1;

	sfname = argv[1];
	if (argc > 2)
		dfname = argv[2];
	else
		dfname = "x.bmp";

	membmp = bmpfileimage_alloc();
	if (!membmp)
		return 1;

	{
		struct BMPFILEREAD *bfr;

		bfr = open_bmp(sfname);
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

		if (bmpfileimage_alloc_image(membmp)) {
			fprintf(stderr,"Failed to allocate memory\n");
			return 1;
		}

		while (read_bmp_line(bfr) == 0) {
			unsigned char BMPFAR *dest = bmpfileimage_row(membmp,(unsigned int)bfr->current_line);
			assert(dest != NULL);

			if (bfr->bpp == 32)
				bitmap_memcpy32to24(dest,bfr->scanline,membmp->width,bfr);
			else
				bitmap_memcpy(dest,bfr->scanline,membmp->stride);
		}

		/* done reading */
		close_bmp(&bfr);
	}

	/* write it out */
	{
		struct BMPFILEWRITE *bfw;
		unsigned char BMPFAR *s;
		unsigned int y;

		bfw = create_write_bmp();
		if (!bfw) {
			fprintf(stderr,"Cannot alloc write bmp\n");
			return 1;
		}

		bfw->bpp = membmp->bpp;
		bfw->width = membmp->width;
		bfw->height = membmp->height;

		if (open_write_bmp(bfw,dfname)) {
			fprintf(stderr,"Cannot write bitmap\n");
			return 1;
		}

		for (y=0;y < membmp->height;y++) {
			s = bmpfileimage_row(membmp,bfw->current_line);
			assert(s != NULL);

			if (write_bmp_line(bfw,s,membmp->stride)) {
				fprintf(stderr,"Scanline write err\n");
				return 1;
			}
		}

		close_write_bmp(&bfw);
	}

	/* free bitmap */
	bmpfileimage_free(&membmp);
	return 0;
#else
	(void)argc;
	(void)argv;
	fprintf(stderr,"Not available for this build\n");
	return 0;
#endif
}

