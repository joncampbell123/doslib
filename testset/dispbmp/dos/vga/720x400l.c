
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

/* VGA 720x480 16-color mode tweaked into 720x400 mode. */

static const char bmpfile[] = "72040016.bmp";
static const unsigned int img_width = 720;
static const unsigned int img_height = 400;
static const unsigned int img_stride = 720 / 8;

#include "dr_4bp.h"
#include "dr_aci.h"

/* CRTC mode parameters.
 * Basically 400-line 256-color mode applied to 640x480 16-color mode */
static const uint16_t crtcchg[] = {
	0x0011,		/* vertical retrace end 0x11: turn off protect */
	0xBF06,		/* vertical total */
	0x1F07,		/* overflow */
	0x4009,		/* maximum scan line register */
	0x9C10,		/* vertical retrace start */
	0x8F12,		/* vertical display end */
	0x9615,		/* start vertical blanking */
	0xB916,		/* end vertical blanking */

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

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 16 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* set 640x480x16 VGA */
	__asm {
		mov	ax,0x0012	; AH=0x00 AL=0x12
		int	0x10
	}
	/* read 0x3CC to determine color vs mono */
	iobase = (inp(0x3CC) & 1) ? 0x3D0 : 0x3B0;

	outp(0x3C2,0x67); /* misc control select 28MHz clock */

	/* CRTC mode set */
	for (i=0;i < (sizeof(crtcchg)/sizeof(crtcchg[0]));i++)
		outpw(iobase+4,crtcchg[i]);

	/* make sure AC palette is identity mapping to eliminate EGA-like default. */
	vga_ac_identity_map();

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
