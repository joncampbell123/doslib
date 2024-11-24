
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

/* VGA 640x350 16-color mode with tweaks to make it a 1bpp mode.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "640350mc.bmp";
static const char bmpfile2[] = "640350m.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 350;
static const unsigned int img_stride = 640 / 8;

#include "dr_1bp.h"
#include "dr_mem.h"
#include "dr_col64.h"

static unsigned char ega4c = 0;

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw,i;

	/* set palette */
	{
		unsigned char ac[16];

		if (ega4c) {
			ac[0] = ac[2] = rgb2ega64(bfr->palette[0].rgbRed,bfr->palette[0].rgbGreen,bfr->palette[0].rgbBlue);
			ac[1] = ac[3] = rgb2ega64(bfr->palette[1].rgbRed,bfr->palette[1].rgbGreen,bfr->palette[1].rgbBlue);

			for (i=0;i < 4;i++) {
				inp(0x3DA);
				outp(0x3C0,i);
				outp(0x3C0,ac[i]);
			}
		}
		else {
			for (i=0;i < bfr->colors;i++)
				ac[i] = rgb2ega64(bfr->palette[i].rgbRed,bfr->palette[i].rgbGreen,bfr->palette[i].rgbBlue);

			for (i=0;i < bfr->colors;i++) {
				inp(0x3DA);
				outp(0x3C0,i);
				outp(0x3C0,ac[i]);
			}
		}
		inp(0x3DA);
		outp(0x3C0,0x20);
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

	/* set 640x350x16 EGA, we're going to tweak it into a 1bpp mode */
	__asm {
		mov	ax,0x0010	; AH=0x00 AL=0x10
		int	0x10
	}

	/* VGA may make this mode the same as 16-color but with a mono palette.
	 * To get what we want, some additional programming is needed. */

	/* 640x350x16 becomes a 2bpp 4-color odd/even mode if the EGA card has 64KB */
	if (ega_memory_size() < 128)
		ega4c = 1;
	else
		ega4c = 0;

	if (ega4c)
		outpw(0x3C4,0x0302); /* write only bitplane 0+1 (map mask) */
	else
		outpw(0x3C4,0x0102); /* write only bitplane 0 (map mask) */

	/* enable only the first bitplane */
	inp(0x3DA);
	outp(0x3C0,0x12);
	outp(0x3C0,0x01);
	inp(0x3DA);
	outp(0x3C0,0x20); /* reenable the display */

	outpw(0x3CE,0x0005); /* write mode 0 (read mode 0) */
	outpw(0x3CE,0xFF08); /* write all bits */

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

