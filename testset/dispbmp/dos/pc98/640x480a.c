
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
 
static const char bmpfile[] = "640480_8.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 480;
static const unsigned int img_stride = 640;

# include "dr_pegc8.h"
#endif

int main() {
#if defined(TARGET_PC98)
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;
	unsigned char sav_mode_al=0,sav_mode_bh=0; /* must be on stack */

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 8 || bfr->colors == 0 || bfr->colors > 256 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* Set PEGC mode 640x480x256 and turn off text. */
	/* NOTE: Get display mode call did not appear until EGC functions were added? */
	__asm {
		mov	ah,0x31		; get display mode
		int	18h
		mov	sav_mode_al,al
		mov	sav_mode_bh,bh
		mov	ah,0x12		; hide cursor
		int	18h
		mov	ah,0x30		; set display mode
		mov	bh,0x30		; 640x480
		mov	al,0x0C		; 31KHz sync
		int	18h
		mov	ah,0x0D		; text layer disable
		int	18h
		mov	ah,0x4D		; 256-color mode
		mov	ch,0x01		; enable
		int	18h
	}

	/* set palette. PEGC 256-color palette entries are 8 bits wide */
	for (i=0;i < bfr->colors;i++) {
		outp(0xA8,i);
		outp(0xAC,bfr->palette[i].rgbRed);
		outp(0xAA,bfr->palette[i].rgbGreen);
		outp(0xAE,bfr->palette[i].rgbBlue);
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

	/* restore normal mode and text */
	__asm {
		mov	ah,0x30		; set display mode
		mov	bh,sav_mode_bh	; whatever the mode was
		mov	al,sav_mode_al	; whatever the mode was
		int	18h
		mov	ah,0x4D		; 256-color mode
		mov	ch,0x00		; disable
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

