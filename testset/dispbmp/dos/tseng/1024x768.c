
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

/* Tseng ET3000/ET4000 640x350 */

static const char bmpfile[] = "10247688.bmp";
static const unsigned int img_width = 1024;
static const unsigned int img_height = 768;

#include "dr_bnkl.h"
#include "detect.h"

static draw_scanline_func_t draw_scanline;

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;

	/* TODO: Tseng ET3K/ET4K detect */
	if (tseng_et3000_detect()) {
		draw_scanline = draw_scanline_et3k;
	}
	else if (tseng_et4000_detect()) {
		draw_scanline = draw_scanline_et4k;
	}
	else {
		fprintf(stderr,"Tseng ET3000/ET4000 not detected\n");
		return 1;
	}

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 8 || bfr->colors == 0 || bfr->colors > 256 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* set 1024x768x256 Tseng ET3k/ET4k */
	/* FIXME: ET4000 only? */
	__asm {
		mov	ax,0x0038	; AH=0x00 AL=0x38
		int	0x10
	}

	/* set palette */
	outp(0x3C8,0);
	for (i=0;i < bfr->colors;i++) {
		outp(0x3C9,bfr->palette[i].rgbRed >> 2);
		outp(0x3C9,bfr->palette[i].rgbGreen >> 2);
		outp(0x3C9,bfr->palette[i].rgbBlue >> 2);
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

