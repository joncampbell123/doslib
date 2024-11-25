
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

/* CGA 320x200 4-color mode.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile1[] = "200p1.bmp";
static const char bmpfile2[] = "200p2.bmp";
static const char bmpfile3[] = "200p3.bmp";
static const unsigned int img_width = 320;
static const unsigned int img_height = 200;
static const unsigned int img_stride = 320 / 4;

#include "dr_2bp.h"

static void cga_palette0() {
	__asm {
		mov	ah,0x0B		; set palette
		mov	bx,0x0100	; set palette, palette 0
		int	10h
	}
}

static void cga_palette1() {
	__asm {
		mov	ah,0x0B		; set palette
		mov	bx,0x0101	; set palette, palette 1
		int	10h
	}
}

static void cga_palette2() {
	outp(0x3D8,0x0E); /* basically 320x200 grayscale which on RGBI makes a third palette */
}

static void convert_scanline(unsigned char *src,unsigned int pixels) {
	unsigned int i,o;

	pixels >>= 1u;

	/* 4bpp packed to 2bpp packed conversion */
	i = o = 0;
	while (i < pixels) {
		register unsigned char b;

		b  = ((src[i+0] >> 4u) & 3u) << 6u;
		b |= ( src[i+0]        & 3u) << 4u;
		b |= ((src[i+1] >> 4u) & 3u) << 2u;
		b |= ( src[i+1]        & 3u);

		src[o] = b; i += 2; o++;
	}
}

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw;

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

	if ((bfr=load(bmpfile1)) == NULL)
		return 1;

	/* set 320x200x4 CGA */
	__asm {
		mov	ax,0x0004	; AH=0x00 AL=0x04
		int	0x10
	}

	cga_palette0();
	display(bfr);
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	if ((bfr=load(bmpfile2)) == NULL)
		return 1;

	cga_palette1();
	display(bfr);
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	if ((bfr=load(bmpfile3)) == NULL)
		return 1;

	cga_palette2();
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

