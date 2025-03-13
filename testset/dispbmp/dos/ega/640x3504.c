
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

/* The 64k EGA 4-color mode is an EGA thing.
 * VGA BIOSes past the early 1990s clearly don't care and have
 * junk or zero bytes for the 640x350 EGA 64k color mode. */
/* advance to the entry for 640x350x4 for 64KB cards */
/* if the mode is there, the first two bytes will be nonzero. */
/* NOTES:
 * - Paradise/Western Digital VGA BIOS: It's not that simple.
 *   There is data there for this mode, but it's invalid. It doesn't
 *   set odd/even mode, but it does set Chain-4?? Why? Every other
 *   entry from 0 to 0x10 has junk data for the fields corresponding
 *   to rows, columns, cell height, and video buffer. To ensure
 *   that we're not using junk data to init, check those fields! */
unsigned int vpt_looks_like_valid_ega64k350_mode(
#if TARGET_MSDOS == 32
	unsigned char *vp
#else
	unsigned char far *vp
#endif
) {
	if (vp[0] == 0 || vp[1] == 0)
		return 0;

	/* Paradise/Western Digital: Why do you have 0x64 0x4A 0x08 0x00 0xFA here? That's not right! Not even the character height!
	 * And why do you list Sequencer Registers 1-4 as 0x01 0x0F 0x03 0x0E? Why Chain-4? Using these values won't produce a valid
	 * mode!
	 *
	 * The correct bytes should be: 0x50 0x18 0x0E 0x00 0x08  0x05 0x0F 0x00 0x00
	 *                              COLS ROWS CHRH -VMEMSIZE  SEQ1 SEQ2 SEQ3 SEQ4
	 *
	 * SEQ1 = 8-dots/chr + shift/load=1 (load every other chr clock)
	 * SEQ2 = map mask all bitplanes
	 * SEQ3 = character map select (doesn't matter)
	 * SEQ4 = chain-4 off, odd/even mode, no "extended memory" meaning memory beyond 64k */
	if (vp[0] == 0x50 && vp[1] == 0x18 && vp[2] == 0x0E && vp[3] == 0x00 && vp[4] == 0x80 &&
		vp[5]/*SEQ1*/ == 0x05 && vp[6]/*SEQ2*/ == 0x0F && /*SEQ3 doesn't matter*/ vp[8]/*SEQ4*/ == 0x00)
		return 1;

	return 0;
}


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

	if (argc > 1) {
		if (!strcmp(argv[1],"-d")) /* show debug info */
			dbg = 1;
		if (!strcmp(argv[1],"-n")) /* don't reprogram the mode */
			nopr = 1;
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
		if (vp != NULL) {
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

		if (!ok) {
			/* We're likely not going to get any help from the VGA BIOS, we'll have to tweak the mode ourselves.
			 * The question becomes then: Will the VGA hardware even support it? A lot of mid to late 1990s hardware still
			 * supports a lot of wild crap including Hercules graphics style interleave display so.... maybe? */
			if (dbg) {
				printf("VGA BIOS+VPT is of no help, manually programming 64kb EGA mode\n");
				getch();
			}

			/* sequencer reset */
			outpw(0x3C4,0x0100);
			/* change offset register to 0x14, instead of 0x28 */
			outpw(0x3D4,0x1413);
			/* change mode control to 0x8B (sync en + div2 + address wrap bit 13) */
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

