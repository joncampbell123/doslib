
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

/* PCjr 320x200 16-color mode.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "32020016.bmp";
static const unsigned int img_width = 320;
static const unsigned int img_height = 200;
static const unsigned int img_stride = 320 / 2;
#if TARGET_MSDOS == 32
static unsigned char *img_vram;
#else
static unsigned char far *img_vram;
#endif
static unsigned char is_pcjr = 0;
static unsigned char is_tandy = 0;

#include "dr_4bp.h"
#include "dr_jrdet.h"
#include "dr_pcjrm.h"

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw;

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0) draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);
}

static struct BMPFILEREAD *load(const char *path) {
	struct BMPFILEREAD *bfr;

	bfr = open_bmp(path);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		close_bmp(&bfr);
		return NULL;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 16 || bfr->palette == 0) {
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

	/* set 320x200x16 PCjr */
	__asm {
		mov	ax,0x0009	; AH=0x00 AL=0x09
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

