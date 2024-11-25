
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

/* Hercules 720x350 2-color mode */

static const char bmpfile[] = "720350.bmp";
static const unsigned int img_width = 720;
static const unsigned int img_height = 350;
static const unsigned int img_stride = 720 / 8;

#include "dr_1bp.h"
#include "dr_det.h"

/* HGC values for HGC graphics */
static const unsigned char hgc_graphics_crtc[12] = {
	0x35,0x2D,0x2E,0x07,
	0x5B,0x02,0x57,0x57,
	0x02,0x03,0x00,0x00
};

static const unsigned char hgc_text_crtc[12] = {
	0x61,0x50,0x52,0x0F,
	0x19,0x06,0x19,0x19,
	0x02,0x0D,0x0D,0x0C
};

static void graphics_on_hgc() {
	unsigned char c;

	outp(0x3B8,0x00); /* turn off video */
	outp(0x3BF,0x01); /* enable setting graphics mode */
	outp(0x3B8,0x02); /* turn on graphics */
	for (c=0;c < 12;c++) {
		outp(0x3B4,c);
		outp(0x3B5,hgc_graphics_crtc[c]);
	}
	outp(0x3B8,0x0A); /* turn on graphics+video */
}

static void graphics_off_hgc() {
	unsigned char c;

	outp(0x3B8,0x00); /* turn off video */
	outp(0x3BF,0x00); /* disable setting graphics mode */
	for (c=0;c < 12;c++) {
		outp(0x3B4,c);
		outp(0x3B5,hgc_text_crtc[c]);
	}
	outp(0x3B8,0x28); /* turn on video and text */
}

static void clear_vram() {
#if TARGET_MSDOS == 32
	memset((void*)0xB0000,0,0x8000);
#else
	_fmemset(MK_FP(0xB000,0x0000),0,0x8000);
#endif
}

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw;

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

	(void)hgc_type;
	if (!herc_detect()) {
		fprintf(stderr,"This program requires a Hercules graphics adapter. MDA will not suffice.\n");
		return 1;
	}

	if ((bfr=load(bmpfile)) == NULL)
		return 1;

	/* Set up Hercules mode.
	 * There is no INT 10h mode for this, it must be manually programmed. */
	clear_vram();
	graphics_on_hgc();

	display(bfr);
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/* Restore text. Again, must manually program.
	 * Also clear the video memory or the graphics become garbage text. */
	clear_vram();
	graphics_off_hgc();
	return 0;
}

