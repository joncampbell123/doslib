
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

/* Tseng ET3000/ET4000 640x480 */
#define ET3K
#define ET4K

static const char bmpfile[] = "640480_1.bmp";
static const char bmpfile2[] = "640480_m.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 480;
static const unsigned int img_stride = 640 / 8;

#include "dr_bnk4.h"
#include "dr_aci.h"
#include "detect.h"

static draw_scanline_func_t draw_scanline;

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw,i;

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
	unsigned int i;

	if (tseng_et3000_detect()) {
		draw_scanline = draw_scanline_et3km;
	}
	else if (tseng_et4000_detect()) {
		draw_scanline = draw_scanline_et4km;
	}
	else {
		fprintf(stderr,"Tseng ET3000/ET4000 not detected\n");
		return 1;
	}

	if ((bfr=load(bmpfile)) == NULL)
		return 1;

	/* set 640x480x16 Tseng ET3k/ET4k (Er, wait, isn't that already mode 0x12?) */
	__asm {
		mov	ax,0x0025	; AH=0x00 AL=0x25
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

	/* AC needs to be an identity map to make palette programming easier */
	vga_ac_identity_map();

	/* set palette */
	outp(0x3C8,0);
	for (i=0;i < bfr->colors;i++) {
		outp(0x3C9,bfr->palette[i].rgbRed >> 2);
		outp(0x3C9,bfr->palette[i].rgbGreen >> 2);
		outp(0x3C9,bfr->palette[i].rgbBlue >> 2);
	}

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

