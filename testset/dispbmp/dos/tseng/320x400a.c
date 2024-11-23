
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

/* VGA 320x400 256-color mode.
 * Standard INT 13h mode but takes advantage of ET3000/ET4000 behavior where you can map it to A0000-BFFFF and
 * it will actually map to the first 128KB instead of the first 64K twice. This is due to the alternate way
 * that chained 256-color works on the ET3K/ET4K cards where instead of just masking off the low 2 bits for
 * bitplane and leaving the rest as-is for planar byte, the ET4K instead masks off the low 2 bits for
 * bitplane and then right shifts down 2 bits to determine planar byte. */

static const char bmpfile[] = "320400_8.bmp";
static const unsigned int img_width = 320;
static const unsigned int img_height = 400;

#include "dr_ch.h"
#include "detect.h"

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;
	uint16_t iobase;

	if (tseng_et3000_detect()) {
	}
	else if (tseng_et4000_detect()) {
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
	outpw(iobase+4,0x4009); /* max scanline 0x09 disable doublescan to get 400 lines */
	outpw(0x3CE,0x0106); /* leave graphics mode enabled, no odd/even mode, map to 0xA0000-0xBFFFF */

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

