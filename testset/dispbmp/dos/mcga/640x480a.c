
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

/* MCGA 640x480 2-color mode.
 * Standard INT 13h mode without any tweaks. */

static const char bmpfile[] = "480mc.bmp";
static const char bmpfile2[] = "480m.bmp";
static const unsigned int img_width = 640;
static const unsigned int img_height = 480;
static const unsigned int img_stride = 640 / 8;

#include "dr_1bp.h"

static void display(struct BMPFILEREAD *bfr) {
	unsigned int dispw;

	/* Set palette.
	 * Unlike VGA, the MCGA has a fixed hardwired way to map monochrome pixels to DAC colors.
	 * There is no Attribute Controller to remap it. Everything appears to map pixels to a
	 * fixed CGA-like RGBI 16-color palette. Therefore, monochrome modes use either color 0
	 * if the bit is zero, or color 15 if the bit is set. */
	outp(0x3C8,0);
	outp(0x3C9,bfr->palette[0].rgbRed >> 2);
	outp(0x3C9,bfr->palette[0].rgbGreen >> 2);
	outp(0x3C9,bfr->palette[0].rgbBlue >> 2);

	outp(0x3C8,15);
	outp(0x3C9,bfr->palette[1].rgbRed >> 2);
	outp(0x3C9,bfr->palette[1].rgbGreen >> 2);
	outp(0x3C9,bfr->palette[1].rgbBlue >> 2);

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0)
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);

	/* If you're curious about the MCGA:
	 *
	 * 64KB of RAM.
	 * The 64KB is mapped twice in the 0xA0000-0xBFFFF range,
	 * except for 0xB0000-0xB7FFF. I assume IBM did this so you could put an MDA alongside it?
	 * This mapping exists at all times, in all video modes.
	 * That means 80x25 text mode on the MCGA is really just the last 32KB half of the same
	 * video memory used by 320x200x256 and 640x480x2 modes.
	 *
	 * The CRTC registers are completely different too. Beyond an initial set of CGA-compatible
	 * registers there are completely different registers in place.
	 *
	 * The MCGA has CGA-compatible registers, though it obviously ignores the colorburst enable.
	 * Beyond CGA and text there is a mode register with bits specifically to select 640x480x2
	 * and 320x200x256. The video modes, according to some documentation, are basically hardcoded
	 * into the MCGA though it does let you program your own.
	 *
	 * The MCGA has the same 15-pin analog connection as VGA and emits the same 400/480-line modes
	 * as VGA, and is therefore perfectly compatible with a VGA monitor. However if you boot the
	 * system up without a monitor attached, all video modes are 200-line 60Hz CGA-like modes with
	 * CGA-like timing. That includes text modes with 8 pixel high fonts, 200-line modes are NOT
	 * double-scanned to 400-line VGA, and the 640x480x2 mode is not available. If you then plug
	 * in a monitor, you get an analog 640x200 signal that your VGA monitor would probably not like.
	 * This decision is made every time the BIOS POSTs, so if you get the 200-line mode and connect
	 * later, just CTRL+ALT+DEL and the BIOS will decide on VGA compatible modes again.
	 *
	 * Speaking of which, all modes other than 640x480x2 are internally run with CGA-like CRTC
	 * parameters. When a VGA monitor is attached, the hardware counts like a 200-line graphics
	 * raster but double scans each line to the monitor. If you poll 0x3DA, you will not see all
	 * 400 lines that way, you will only see the internal 200 lines prior to double scanning.
	 * If you intend to do any Demoscene-ish copper bars, know you will only be able to change the
	 * palette every other line. Even text mode, which appears on screen using an 8x16 font if a
	 * VGA monitor is attached, is still internally counting scanlines as if a 200-line mode.
	 *
	 * Speaking of "copper bars", MCGA has the one and only video mode where copper bar tricks do
	 * not work: The 640x480x2 mode. This is the only video mode where the only time the DAC
	 * latches the palette contents is somewhere during start of vertical display (or vsync? not
	 * sure). If you write new DAC colors, they do not take effect until vsync. All other MCGA
	 * modes can allow "cooper bars".
	 *
	 * Despite being a weird superset of CGA with 64KB of RAM, the hardware does have several
	 * I/O ports that are the same as they are on VGA. That includes the DAC palette registers
	 * (complete the DAC snow if you change during active display), and the PEL mask. However
	 * unlike the VGA, the misc register is not present and the MCGA will always respond to
	 * ports 0x3D0-0x3DF, it cannot be directed to the "monochrome" I/O ports 0x3B0-0x3BF.
	 * It is after all the Multi-COLOR Graphics Adapter, right? This also means of course that
	 * if an MDA card were in the system, the MDA could take 0x3B0-0x3BF without conflict.
	 *
	 * According to the register dump on Hackipedia, the MCGA, just like the CGA, has aliases
	 * of the CRTC ports in the first 8 I/O ports. Same as the CGA, I/O ports 0x3D0-0x3D7
	 * responds exactly like aliases of ports 0x3D4 and 0x3D5. It also has CGA-compatible
	 * registers at ports 0x3D8 and 0x3D9.
	 *
	 * There are unknown registers in the 0x3DC-0x3DF range that do strange things, though the
	 * bits are also involved with the character generatora
	 *
	 * There is literally one bit documented as "bit 8" of vertical total that appears to
	 * actually control the double scanning. Such as playing with the value can change the
	 * text mode to 8x16. If the BIOS is not there to modify the font bitmaps, you get the
	 * 8x8 squeezed at the top of the cell with whatever junk (often the 8x16 font) on the
	 * bottom half. This bit seems crucial in the selection between 200-line CGA-like and
	 * 400-line VGA-like monitor output.
	 *
	 * The character generator on the MCGA is not fixed in ROM as it is on CGA. You CAN change
	 * it. However there's no font RAM to write to, and the BIOS functions don't seem to work.
	 * The way it works apparently is that the MCGA is using a chip that's normally in a mode
	 * where, given the data read from memory, it reads from the character generator. But you
	 * can put this chip in a mode where the byte it read from VRAM is instead WRITTEN to the
	 * character generator memory in order to change the font. The BIOS does this to select
	 * between 8x16 (400-line) and 8x8 (200-line) text modes. So setting the character set font
	 * involves then writing the font bitmaps to video memory, switching this chip around like
	 * that, and letting the chip write the new font as it reads video memory.
	 *
	 * Be thankful for this dump of info as information on the MCGA is very sparse and hard to
	 * find. This information was compiled from very brief and sparse documentation from the
	 * "PC & PS/2 Video Systems" book and from poking around an actual IBM PS/2 system with
	 * MCGA hardware. By the way, if you believed the book, you would somehow set the MCGA font
	 * from some weird indirect interleaved version of font bitmaps which is wrong. What the
	 * author probably saw was the temporary data placed by the BIOS for the "reverse character
	 * generator" mode of setting font RAM to work. */
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

	if ((bfr=load(bmpfile)) == NULL)
		return 1;

	/* set 640x480x2 MCGA */
	__asm {
		mov	ax,0x0011	; AH=0x00 AL=0x11
		int	0x10
	}

	display(bfr);
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/////

	if ((bfr=load(bmpfile2)) == NULL)
		return 1;

	display(bfr);
	close_bmp(&bfr);

	/////

	/* wait for ENTER */
	while (getch() != 13);

	/* restore text */
	__asm {
		mov	ax,0x0003	; AH=0x00 AL=0x03
		int	0x10
	}
	return 0;
}

