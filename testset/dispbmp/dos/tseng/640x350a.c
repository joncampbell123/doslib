
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

static const char bmpfile[] = "640350_8.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 350;

#include "dr_bnkl.h"

static draw_scanline_func_t draw_scanline;

//////

int tseng_et3000_detect() {
	unsigned char old,val;
	int base;

	old = inp(0x3CD);
	outp(0x3CD,old ^ 0x3F);
	val = inp(0x3CD);
	outp(0x3CD,old);
	if (val != (old ^ 0x3F)) return 0;

	if (inp(0x3CC) & 1)	base = 0x3D4;
	else			base = 0x3B4;
	outp(base,0x1B);
	old = inp(base+1);
	outp(base+1,old ^ 0xFF);
	val = inp(base+1);
	outp(base+1,old);
	if (val != (old ^ 0xFF)) return 0;

	/* ET3000 detected */
	return 1;
}

void tseng_et4000_extended_set_lock(int x) {
	if (x) {
		if (inp(0x3CC) & 1)	outp(0x3D8,0x29);
		else			outp(0x3B8,0x29);
		outp(0x3BF,0x01);
	}
	else {
		outp(0x3BF,0x03);
		if (inp(0x3CC) & 1)	outp(0x3D8,0xA0);
		else			outp(0x3B8,0xA0);
	}
}

int tseng_et4000_detect() {
	unsigned char new,old,val;
	int base;

	/* extended register enable */
	tseng_et4000_extended_set_lock(/*unlock*/0);

	old = inp(0x3CD);
	outp(0x3CD,0x55);
	val = inp(0x3CD);
	outp(0x3CD,old);
	if (val != 0x55) return 0;

	if (inp(0x3CC) & 1)	base = 0x3D4;
	else			base = 0x3B4;
	outp(base,0x33);
	old = inp(base+1);
	new = old ^ 0x0F;
	outp(base+1,new);
	val = inp(base+1);
	outp(base+1,old);
	if (val != new) return 0;

	/* extended register lock */
	tseng_et4000_extended_set_lock(/*lock*/1);

	/* ET4000 detected */
	return 1;
}

//////

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

	/* set 640x350x256 Tseng ET3k/ET4k */
	__asm {
		mov	ax,0x002D	; AH=0x00 AL=0x2D
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

