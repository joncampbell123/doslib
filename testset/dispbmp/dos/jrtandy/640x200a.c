
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

/* PCjr 640x200 4-color mode.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "6402004.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 200;
static const unsigned int img_stride = (640 / 8) * 2;
#if TARGET_MSDOS == 32
static unsigned char *img_vram;
#else
static unsigned char far *img_vram;
#endif
static unsigned char is_pcjr = 0;
static unsigned char is_tandy = 0;

#include "dr_2bp4.h"
#include "dr_col16.h"
#include "dr_jrdet.h"
#include "dr_pcjrm.h"

static void jr_setpalette(unsigned char i,unsigned char c) {
	inp(0x3DA);
	if (is_pcjr) {
		outp(0x3DA,0x10 + i);
		outp(0x3DA,c);

		/* NTS: DOSBox machine=pcjr emulation seems to imply that writing palette
		 *      registers blanks the display. Do dummy write to un-blank the display. */
		outp(0x3DA,0x00);
	}
	else {
		outp(0x3DA,0x10 + i);
		outp(0x3DE,c);	/* NTS: This works properly on Tandy [at least DOSBox] */
	}
}

static void convert_scanline(unsigned char *src,unsigned int pixels) {
	unsigned int i,o;

	pixels >>= 1u;

	/* 4bpp packed to 2x1bpp conversion. 640x200x4 on PCjr+Tandy is weird. */
	i = o = 0;
	while (i < pixels) {
		register unsigned char a,b;

		a  = ((src[i+0] >> 4u) & 1u) << 7u;
		a |= ( src[i+0]        & 1u) << 6u;
		a |= ((src[i+1] >> 4u) & 1u) << 5u;
		a |= ( src[i+1]        & 1u) << 4u;
		a |= ((src[i+2] >> 4u) & 1u) << 3u;
		a |= ( src[i+2]        & 1u) << 2u;
		a |= ((src[i+3] >> 4u) & 1u) << 1u;
		a |= ( src[i+3]        & 1u);

		b  = ((src[i+0] >> 5u) & 1u) << 7u;
		b |= ((src[i+0] >> 1u) & 1u) << 6u;
		b |= ((src[i+1] >> 5u) & 1u) << 5u;
		b |= ((src[i+1] >> 1u) & 1u) << 4u;
		b |= ((src[i+2] >> 5u) & 1u) << 3u;
		b |= ((src[i+2] >> 1u) & 1u) << 2u;
		b |= ((src[i+3] >> 5u) & 1u) << 1u;
		b |= ((src[i+3] >> 1u) & 1u);

		src[o+0] = a;
		src[o+1] = b;
		i += 4;
		o += 2;
	}
}

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw,i;

	/* Load palette.
	 * Colors 0...3 map to palette 0...3 */
	for (i=0;i < 4;i++)
		jr_setpalette(i,rgb2cga16(bfr->palette[i].rgbRed,bfr->palette[i].rgbGreen,bfr->palette[i].rgbBlue));

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0) {
		convert_scanline(bfr->scanline,dispw);
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);
	}
}

static struct BMPFILEREAD *load(const char *path) {
	struct BMPFILEREAD *bfr;

	bfr = open_bmp(path);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		close_bmp(&bfr);
		return NULL;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 4 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		close_bmp(&bfr);
		return NULL;
	}

	return bfr;
}

int main() {
	struct BMPFILEREAD *bfr;

	if (detect_pcjr()) {
		is_pcjr = 1;
	}
	else if (detect_tandy()) {
		is_tandy = 1;
	}
	else {
		fprintf(stderr,"This program requires a PCjr or Tandy system\n");
		return 1;
	}

	img_vram = get_pcjr_mem();
	if (img_vram == NULL) {
		fprintf(stderr,"Unable to locate PCjr/Tandy RAM\n");
		return 1;
	}

	if ((bfr=load(bmpfile)) == NULL)
		return 1;

	/* set 640x200x4 PCjr */
	__asm {
		mov	ax,0x000A	; AH=0x00 AL=0x0A
		int	0x10
	}

	/* the video mode probably changed the pointer */
	img_vram = get_pcjr_mem();
	if (img_vram == NULL) {
		fprintf(stderr,"Unable to locate PCjr/Tandy RAM\n");
		return 1;
	}

	display(bfr);
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

