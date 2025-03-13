
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

/* EGA 640x350 16-color mode on VGA.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "640350_4.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 350;
static const unsigned int img_stride = 640 / 8;

#define EGA4COLOR

#include "dr_4bp.h"
#include "dr_mem.h"
#include "dr_col64.h"
#include "dr_vpt.h"

static unsigned char translate_nibble4(unsigned char b) {
	return ((b & 1) ? 3 : 0) + ((b & 2) ? 12 : 0);
}

static void translate_scanline(unsigned char *b,unsigned int w) {
	unsigned int x;

	w = (w + 1u) >> 1u;
	for (x=0;x < w;x++) {
		/* convert each nibble [0 1 2 3] -> [0 3 12 15] */
		b[x] = (translate_nibble4(b[x] >> 4u) << 4u) + translate_nibble4(b[x] & 0xFu);
	}
}

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 16 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* set 640x350x16 EGA */
	__asm {
		mov	ax,0x0010	; AH=0x00 AL=0x10
		int	0x10
	}

	/* 640x350x4 only happens for 64KB
	 * If we WANT 640x350x4 in any other case we have to set it up ourselves from the Video Parameter Table */
	if (ega_memory_size() >= 128) {
#if TARGET_MSDOS == 32
		unsigned char *vp;
#else
		unsigned char far *vp;
#endif

		vp = find_vpt();
		if (vp != NULL) {
			/* advance to the entry for 640x350x4 for 64KB cards */
			/* if the mode is there, the first two bytes will be nonzero */
			vp += 0x40*0x10;
			if (vp[0] != 0 && vp[1] != 0) {
				printf("VPTable entry available!\n");
				apply_vpt_mode(vp);
			}
		}
	}

	/* set palette */
	{
		unsigned char ac[16];

		for (i=0;i < 16;i++) {
			unsigned int j = (i & 1u) + ((i & 4u) >> 1u);
			ac[i] = rgb2ega64(bfr->palette[j].rgbRed,bfr->palette[j].rgbGreen,bfr->palette[j].rgbBlue);
		}

		for (i=0;i < 16;i++) {
			inp(0x3DA);
			outp(0x3C0,i);
			outp(0x3C0,ac[i]);
		}
		inp(0x3DA);
		outp(0x3C0,0x20);
		inp(0x3DA);
	}

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0) {
		translate_scanline(bfr->scanline,dispw);
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);
	}

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

