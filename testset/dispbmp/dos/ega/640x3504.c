
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

int main(int argc,char **argv) {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;
	unsigned char dbg = 0;
	unsigned char nopr = 0;
	unsigned char fopr = 0;

	for (i=1;i < argc;i++) {
		if (!strcmp(argv[i],"-d")) /* show debug info */
			dbg = 1;
		else if (!strcmp(argv[i],"-n")) /* don't reprogram the mode */
			nopr = 1;
		else if (!strcmp(argv[i],"-f")) /* force program the mode, don't even use the VPT */
			fopr = 1;
	}

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

	if (nopr) {
		/* if asked, do nothing.
		 * the way this code works it still draws correctly even if the full 16 colors
		 * and 4 bitplanes are available and no reprogramming is done */
	}
	/* 640x350x4 only happens for 64KB
	 * If we WANT 640x350x4 in any other case we have to set it up ourselves from the Video Parameter Table */
	else if (ega_memory_size() >= 128) {
#if TARGET_MSDOS == 32
		unsigned char *vp;
#else
		unsigned char far *vp;
#endif
		unsigned char ok = 0;

		vp = find_vpt();
		if (vp != NULL && !fopr) {
			if (dbg) {
				printf("VPTable found!\n");
				getch();
			}

			vp += 0x40*0x10;
			if (vpt_looks_like_valid_ega64k350_mode(vp)) {
				if (dbg) {
					printf("VPTable entry available and validated!\n");
					getch();
				}

				apply_vpt_mode(vp);
				ok = 1;
			}
		}

		if (!ok && fopr) {
			/* We're likely not going to get any help from the VGA BIOS, we'll have to tweak the mode ourselves.
			 * The question becomes then: Will the VGA hardware even support it? A lot of mid to late 1990s hardware still
			 * supports a lot of wild crap including Hercules graphics style interleave display so.... maybe? */
			/* NOTES: Paradise/Western Digital: This doesn't work. You only get every other column!
			 *        The chipset seems to completely ignore the shift/load rate bit (and the VGA shift four bit)
			 *        in graphics modes, which prevents this from working. Yet for some reason, those bits DO have
			 *        a visual effect in text mode. Hm.
			 *
			 * So basically it's best to assume that if the VPT doesn't provide the entry, it's probably a VGA
			 * chipset from the 1990s that doesn't care to emulate this weird mode anyway.
			 *
			 * This code displays the image correctly anyway without reprogramming the registers, so from now on, this
			 * manual tweaking is only offered if you want it, otherwise, no programming */
			if (dbg) {
				printf("VGA BIOS+VPT is of no help, manually programming 64kb EGA mode\n");
				getch();
			}

			/* sequencer reset */
			outpw(0x3C4,0x0100);
			/* change offset register to 0x14, instead of 0x28 */
			outpw(0x3D4,0x1413);
			/* change mode control to 0x8B (sync en + div2 + address wrap bit 13 + word mode) */
			outpw(0x3D4,0x8B17);
			/* odd/even mode */
			outpw(0x3CE,0x1005); // odd-even mode
			outpw(0x3CE,0x0706); // odd-even mode, graphics at 0xA0000-0xAFFFF
			outpw(0x3CE,0x0F07); // color don't care
			/* enable bitplanes 0 and 2 only */
			inp(0x3DA);
			outp(0x3C0,0x12 | 0x20);
			outp(0x3C0,0x05);
			/* sequencer turn on odd/even and half clock, only 64k */
			outpw(0x3C4,0x0501);
			outpw(0x3C4,0x0004);
			/* sequencer restore */
			outpw(0x3C4,0x0300);

			ok = 1;
		}

		if (!ok) {
			if (dbg) {
				printf("Not reprogramming the mode, no need. Use -f if that is wanted.\n");
				getch();
			}
		}
	}
	else {
		if (dbg) {
			printf("EGA card has 64kb and will already provide 640x350 4-color mode, no programming required\n");
			getch();
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

