
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <i86.h>

#if defined(TARGET_PC98)
# include "libbmp.h"
 
static const char bmpfile[] = "640200_3.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 200;
static const unsigned int img_stride = 640 / 8; /* 8-color planar, 1bpp x 3 bitplanes */

# include "dr_dig8.h"
#endif

int main() {
#if defined(TARGET_PC98)
	struct BMPFILEREAD *bfr;
	unsigned int dispw;
	unsigned char sav_mode_al=0,sav_mode_bh=0; /* must be on stack */

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 8 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* Set 16-color mode */
	/* NOTE: PC-98 systems in the traditional 400-line raster can be in one of two sync configurations.
	 *       One is a 640x400 70Hz mode that is fully compatible with standard VGA monitors.
	 *       The other is a 640x400 56Hz mode that is incompatible with VGA but was standard for the platform (24KHz hsync),
	 *       though if you own an NEC brand VGA monitor it may support the mode through the VGA cable regardless.
	 *       To support both, this code queries the mode, then sets the mode without changing the AL register. */
	/* NOTE: PC-98 supports 8/16-color 200-line and 400-line modes, because it faithfully emulates
	 *       registers of their particular video chipset that allows that to happen. However once
	 *       you go into 256-color PEGC mode, some functions disappear and it is not possible to
	 *       "double scan" 256-color mode to get 640x240 or 640x200 modes. Believe, me, I tried. */
	/* NOTE: Get display mode call did not appear until EGC functions were added? */
	__asm {
		mov	ah,0x31		; get display mode
		int	18h
		mov	sav_mode_al,al
		mov	sav_mode_bh,bh
		mov	ah,0x12		; hide cursor
		int	18h
		mov	ah,0x30		; set display mode
		mov	bh,0x01		; 640x200 with 80x25 text
					; Do not change AL, keep the sync the same
					; FIXME: The system could be running a 80x25 text mode at 640x480, perhaps
		int	18h
		mov	ah,0x0D		; text layer disable
		int	18h
		mov	ah,0x40		; graphics layer enable
		int	18h

		mov	al,0x00		; disable 16-color
		out	6Ah,al
		mov	al,0x06		; disable EGC
		out	6Ah,al
	}

	/* set palette. 8-color palette entries are 3 bits each packed together */
	outp(0xAE,
		(rgb2dig8(bfr->palette[0].rgbRed,bfr->palette[0].rgbGreen,bfr->palette[0].rgbBlue) << 4u) +
		 rgb2dig8(bfr->palette[4].rgbRed,bfr->palette[4].rgbGreen,bfr->palette[4].rgbBlue));
	outp(0xAA,
		(rgb2dig8(bfr->palette[1].rgbRed,bfr->palette[1].rgbGreen,bfr->palette[1].rgbBlue) << 4u) +
		 rgb2dig8(bfr->palette[5].rgbRed,bfr->palette[5].rgbGreen,bfr->palette[5].rgbBlue));
	outp(0xAC,
		(rgb2dig8(bfr->palette[2].rgbRed,bfr->palette[2].rgbGreen,bfr->palette[2].rgbBlue) << 4u) +
		 rgb2dig8(bfr->palette[6].rgbRed,bfr->palette[6].rgbGreen,bfr->palette[6].rgbBlue));
	outp(0xA8,
		(rgb2dig8(bfr->palette[3].rgbRed,bfr->palette[3].rgbGreen,bfr->palette[3].rgbBlue) << 4u) +
		 rgb2dig8(bfr->palette[7].rgbRed,bfr->palette[7].rgbGreen,bfr->palette[7].rgbBlue));

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0)
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);

	/* done reading */
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/* restore normal mode and text */
	__asm {
		mov	ah,0x30		; set display mode
		mov	bh,sav_mode_bh	; whatever the mode was
		mov	al,sav_mode_al	; whatever the mode was
		int	18h
		mov	ah,0x41		; graphics layer disable
		int	18h
		mov	ah,0x0C		; text layer enable
		int	18h
		mov	ah,0x11		; show cursor
		int	18h
	}
#else
	fprintf(stderr,"This is only for NEC PC-98\n");
#endif
	return 0;
}

