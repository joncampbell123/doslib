
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>

#include "libbmp.h"

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <windows.h>
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
#if defined(ENABLE_BMPFILEIMAGE)
	struct BMPFILEREAD *bfr = NULL;
	struct BMPFILEWRITE *bfw = NULL;
	struct BMPFILEIMAGE *membmp = NULL;
	const char *sfname,*dfname;

	(void)envp;

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

		bfr = open_bmp(sfname);
		if (bfr == NULL) {
			printf("Failed to open BMP, errno %s\n",strerror(errno));
			return 1;
		}

		membmp->bpp = bfr->bpp;
		membmp->width = bfr->width;
		membmp->height = bfr->height;

		if (bmpfileimage_alloc_image(membmp)) {
			printf("Failed to allocate memory\n");
			return 1;
		}

		while (read_bmp_line(bfr) == 0) {
			unsigned char BMPFAR *dest = bmpfileimage_row(membmp,(unsigned int)bfr->current_line);
			assert(dest != NULL);
			bitmap_memcpy(dest,bfr->scanline,membmp->stride);
		}
	}

	/* write it out */
	{
		unsigned char BMPFAR *s;
		unsigned int y;

		bfw = create_write_bmp();
		if (!bfw) {
			printf("Cannot alloc write bmp\n");
			return 1;
		}

		bfw->bpp = membmp->bpp;
		bfw->width = membmp->width;
		bfw->height = membmp->height;

		if (createpalette_write_bmp(bfw)) {
			printf("Cannot create palette write bmp\n");
			return 1;
		}

		if (bfw->palette && bfr->palette && bfr->colors <= bfw->colors) {
			bfw->colors_used = bfr->colors;
			memcpy(bfw->palette,bfr->palette,sizeof(struct BMPPALENTRY) * bfw->colors_used);
		}

		if (open_write_bmp(bfw,dfname)) {
			printf("Cannot write bitmap\n");
			return 1;
		}

		for (y=0;y < membmp->height;y++) {
			s = bmpfileimage_row(membmp,bfw->current_line);
			assert(s != NULL);

			if (write_bmp_line(bfw,s,membmp->stride)) {
				printf("Scanline write err\n");
				return 1;
			}
		}

		close_write_bmp(&bfw);
		close_bmp(&bfr);
	}

	/* free bitmap */
	bmpfileimage_free(&membmp);
	return 0;
#else
	(void)argc;
	(void)argv;
	printf("Not available for this build\n");
	return 0;
#endif
}

