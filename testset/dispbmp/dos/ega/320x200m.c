
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

/* VGA 320x200 16-color mode with tweaks to make it a 1bpp mode.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "320200mc.bmp";
static const char bmpfile2[] = "320200m.bmp";
static const unsigned int img_width = 320;
static const unsigned int img_height = 200;
static const unsigned int img_stride = 320 / 8;

#include "dr_1bp.h"
#include "dr_col16.h"

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw,i;

	/* set palette */
	{
		unsigned char ac[16];

		for (i=0;i < bfr->colors;i++)
			ac[i] = rgb2ega16(bfr->palette[i].rgbRed,bfr->palette[i].rgbGreen,bfr->palette[i].rgbBlue);

		for (i=0;i < bfr->colors;i++) {
			inp(0x3DA);
			outp(0x3C0,i);
			outp(0x3C0,ac[i]);
		}
		inp(0x3DA);
		outp(0x3C0,0x20 | ac[0]);
	}

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0)
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);
}

static struct BMPFILEREAD *load(const char *path) {
	struct BMPFILEREAD *bfr;

	bfr = open_bmp(path);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		close_bmp(&bfr);
		return NULL;
	}
	if (bfr->bpp != 1 || bfr->colors == 0 || bfr->colors > 2 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		close_bmp(&bfr);
		return NULL;
	}

	return bfr;
}

int main() {
	struct BMPFILEREAD *bfr;

	if ((bfr=load(bmpfile)) == NULL)
		return 1;

	/* set 320x200x16 EGA */
	__asm {
		mov	ax,0x000D	; AH=0x00 AL=0x0D
		int	0x10
	}

	/* VGA may make this mode the same as 16-color but with a mono palette.
	 * To get what we want, some additional programming is needed. */
	outpw(0x3C4,0x0102); /* write only bitplane 0 (map mask) */
	outpw(0x3CE,0x0005); /* write mode 0 (read mode 0) */
	outpw(0x3CE,0xFF08); /* write all bits */

	/* enable only the first bitplane */
	inp(0x3DA);
	outp(0x3C0,0x12);
	outp(0x3C0,0x01);
	inp(0x3DA);
	outp(0x3C0,0x20); /* reenable the display */

	display(bfr);
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/////

	if ((bfr=load(bmpfile2)) == NULL)
		return 1;

	display(bfr);
	close_bmp(&bfr);

	/////

	/* wait for ENTER */
	while (getch() != 13);

	/* restore text */
	__asm {
		mov	ax,0x0003	; AH=0x00 AL=0x03
		int	0x10
	}
	return 0;
}

