
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

#define ET3K
#define ET4K

/* VGA 360x600 256-color mode.
 * Standard INT 13h mode but we use ET4000 bank switching instead of unchained.
 * 360x600 has too many pixels to fit within the 128KB 0xA0000-0xBFFFF window. */

static const char bmpfile[] = "360600_8.bmp";
static const unsigned int img_width = 360;
static const unsigned int img_height = 600;

#include "dr_bnkl.h"
#include "detect.h"

static draw_scanline_func_t draw_scanline;

/* CRTC mode parameters.
 * Basically 800x600 mode applied to 256-color mode */
static const uint16_t crtcchg[] = {
	0x0011,		/* vertical retrace end 0x11: turn off protect */
	0x7806,		/* vertical total 0x06 from 640x480 */
	0xF007,		/* overflow 0x07 from same */
	0x6009,		/* maximum scan line register 0x09 */
	0x5C10,		/* vertical retrace start 0x10 */
	0x5712,		/* vertical display end 0x12 */
	0x5B15,		/* start vertical blanking 0x15 */
	0x7516,		/* end vertical blanking 0x16 */

	0x6B00,		/* horizontal total */
	0x5901,		/* end horizontal display */
	0x5A02,		/* start horizontal blanking */
	0x8E03,		/* end horizontal blanking */
	0x5E04,		/* start horiztonal retrace */
	0x8A05,		/* end horiztonal retrace */

	0x2D13,		/* offset register */
	0x8E11		/* vertical retrace end 0x11 which also sets protect, which is why this is the last write */
};

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;
	uint16_t iobase;

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

	/* set 320x200x256 VGA */
	__asm {
		mov	ax,0x0013	; AH=0x00 AL=0x13
		int	0x10
	}
	/* read 0x3CC to determine color vs mono */
	iobase = (inp(0x3CC) & 1) ? 0x3D0 : 0x3B0;

	/* CRTC byte mode. If this is not Tseng it will make a mess of your display. */
	outpw(iobase+4,0x0014); /* underline register 0x14 turn off doubleword addressing */
	outpw(iobase+4,0xE317); /* crtc mode control 0x17 SE=1 bytemode AW=1 DIV2=0 SLDIV=0 MAP14=1 MAP13=1 */
	outpw(iobase+4,0x2813); /* offset register 0x13: 0x28 for 320 pixels across */

	/* CRTC mode set */
	for (i=0;i < (sizeof(crtcchg)/sizeof(crtcchg[0]));i++)
		outpw(iobase+4,crtcchg[i]);

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

