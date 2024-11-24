
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

/* EGA 640x350 16-color mode on VGA.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "64035016.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 350;
static const unsigned int img_stride = 640 / 8;

#include "dr_4bp.h"
#include "dr_mem.h"
#include "dr_col64.h"

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 16 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* 640x350x16 requires at least 128KB onboard memory */
	if (ega_memory_size() < 128) {
		fprintf(stderr,"This program requires an EGA card with at least 128KB of video RAM\n");
		return 1;
	}

	/* set 640x350x16 EGA */
	__asm {
		mov	ax,0x0010	; AH=0x00 AL=0x10
		int	0x10
	}

	/* set palette */
	{
		unsigned char ac[16];

		for (i=0;i < bfr->colors;i++)
			ac[i] = rgb2ega64(bfr->palette[i].rgbRed,bfr->palette[i].rgbGreen,bfr->palette[i].rgbBlue);

		for (i=0;i < bfr->colors;i++) {
			inp(0x3DA);
			outp(0x3C0,i);
			outp(0x3C0,ac[i]);
		}
		inp(0x3DA);
		outp(0x3C0,0x20);
	}

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0)
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);

	/* done reading */
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/* restore text */
	__asm {
		mov	ax,0x0003	; AH=0x00 AL=0x03
		int	0x10
	}
	return 0;
}

