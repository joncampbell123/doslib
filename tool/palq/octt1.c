
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#if defined(LINUX)
# include <time.h>
#endif

#include "libbmp.h"

struct rgb_t {
	unsigned int	r,g,b;
};

int main(int argc,char **argv) {
	struct BMPFILEIMAGE *membmp = NULL;
	unsigned int target_colors = 256;
	struct rgb_t *palette;
	unsigned int i;

	if (argc < 3)
		return 1;

	membmp = bmpfileimage_alloc();
	if (!membmp)
		return 1;

	if (argc > 3) {
		fprintf(stderr,"%s\n",argv[3]);
		target_colors = strtoul(argv[3],NULL,0);
		if (target_colors == 0u || target_colors > 256u)
			return 1;
	}

	palette = malloc(sizeof(struct rgb_t) * 256);
	if (!palette)
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

		if (bmpfileimage_alloc_image(membmp)) {
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

#if defined(LINUX)
	srand(time(NULL));
#endif

	/* random palette */
	for (i=0;i < target_colors;i++) {
		palette[i].r = (unsigned char)rand();
		palette[i].g = (unsigned char)rand();
		palette[i].b = (unsigned char)rand();
	}

	/* write it out */
	{
		struct BMPFILEWRITE *bfw;
		unsigned char *s;
		unsigned int y;

		bfw = create_write_bmp();
		if (!bfw) {
			fprintf(stderr,"Cannot alloc write bmp\n");
			return 1;
		}

		bfw->bpp = 8; // TODO: 2 colors = 1bpp  3-16 colors = 4bpp   anything else 8bpp
		bfw->width = membmp->width;
		bfw->height = membmp->height;
		bfw->colors_used = target_colors;

		if (createpalette_write_bmp(bfw)) {
			fprintf(stderr,"Cannot make write bmp palette\n");
			return 1;
		}

		if (bfw->palette) {
			bfw->colors_used = target_colors;
			for (i=0;i < target_colors;i++) {
				bfw->palette[i].rgbRed = palette[i].r;
				bfw->palette[i].rgbGreen = palette[i].g;
				bfw->palette[i].rgbBlue = palette[i].b;
			}
		}

		if (open_write_bmp(bfw,argv[2])) {
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
	free(palette);
	return 0;
}

