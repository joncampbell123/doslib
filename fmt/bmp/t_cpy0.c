
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
	struct BMPFILEREAD *bfr;
	struct BMPFILEWRITE *bfw;
	const char *sfname,*dfname;

	if (argc < 2)
		return 1;

	sfname = argv[1];
	if (argc > 2)
		dfname = argv[2];
	else
		dfname = "x.bmp";

	bfr = open_bmp(sfname);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (!(bfr->bpp == 15 || bfr->bpp == 16 || bfr->bpp == 24 || bfr->bpp == 32)) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	bfw = create_write_bmp();
	if (!bfw) {
		fprintf(stderr,"Cannot alloc write bmp\n");
		return 1;
	}

	bfw->bpp = bfr->bpp;
	bfw->width = bfr->width;
	bfw->height = bfr->height;

	if (open_write_bmp(bfw,dfname)) {
		fprintf(stderr,"Cannot write bitmap\n");
		return 1;
	}

	while (read_bmp_line(bfr) == 0) {
		if (write_bmp_line(bfw,bfr->scanline,bfr->scanline_size)) {
			fprintf(stderr,"Scanline write err\n");
			return 1;
		}
	}

	/* done */
	close_bmp(&bfr);
	close_write_bmp(&bfw);
	return 0;
}

