#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/dos/doswin.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

void vga_splitscreen(unsigned int v) {
	unsigned char c;

	/* FIXME: Either DOSBox 0.74 got EGA emulation wrong, or we really do have to divide the line count by 2. */
	/*        Until then leave the value as-is */
	/*  TODO: Didn't Mike Abrash or other programming gurus mention a bug or off-by-1 error with the EGA linecount? */

	vga_write_CRTC(0x18,v);
	if (vga_state.vga_flags & VGA_IS_VGA) {
		c = vga_read_CRTC(0x07);
		vga_write_CRTC(0x07,(c & (~0x10)) | (((v>>8)&1) << 4));
		c = vga_read_CRTC(0x09);
		vga_write_CRTC(0x09,(c & (~0x40)) | (((v>>9)&1) << 6));
	}
	else {
		c = 0x1F; /* since most modes have > 256 lines this is as good a guess as any */
		vga_write_CRTC(0x07,(c & (~0x10)) | (((v>>8)&1) << 4));
	}
}

