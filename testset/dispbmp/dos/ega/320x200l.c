
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

/* EGA 320x200 16-color mode on VGA.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "32020016.bmp";
static const unsigned int img_width = 320;
static const unsigned int img_height = 200;
static const unsigned int img_stride = 320 / 8;

#include "dr_4bp.h"

int main() {
	struct BMPFILEREAD *bfr;
	unsigned int dispw;

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp != 4 || bfr->colors == 0 || bfr->colors > 16 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* set 320x200x16 EGA */
	__asm {
		mov	ax,0x000D	; AH=0x00 AL=0x0D
		int	0x10
	}

	/* bitmap palette is already the RGBI of the EGA mode */

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

