
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
	struct BMPFILEREAD *bfr;
	struct BMPFILEWRITE *bfw;
	const char *sfname,*dfname;

	(void)envp;

	if (argc < 2)
		return 1;

	sfname = argv[1];
	if (argc > 2)
		dfname = argv[2];
	else
		dfname = "x.bmp";

	bfr = open_bmp(sfname);
	if (bfr == NULL) {
		printf("Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}

	bfw = create_write_bmp();
	if (!bfw) {
		printf("Cannot alloc write bmp\n");
		return 1;
	}

	bfw->bpp = bfr->bpp;
	bfw->width = bfr->width;
	bfw->height = bfr->height;

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

	while (read_bmp_line(bfr) == 0) {
		if (write_bmp_line(bfw,bfr->scanline,bfr->scanline_size)) {
			printf("Scanline write err\n");
			return 1;
		}
	}

	/* done */
	close_bmp(&bfr);
	close_write_bmp(&bfw);
	return 0;
}

