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

/* WARNING: [At least in DOSBox 0.74] do not call this for any CGA or EGA graphics modes.
 *          It seems to trigger a false mode change and alphanumeric mode */
void vga_relocate_crtc(unsigned char color) {
	unsigned char moc = 0;

	/* this code assumes color == 0 or color == 1 */
	color = (color != 0)?1:0;

	/* VGA: Easy, read the register, write it back */
	if (vga_state.vga_flags & VGA_IS_VGA) {
		moc = inp(0x3CC);
		moc &= 0xFE; /* clear bit 0, respond to 0x3Bx */
		outp(0x3C2,moc | color);
	}
	else if (vga_state.vga_flags & VGA_IS_EGA) {
		/* EGA: We can't read it, but we can write it from our best guess */
		outp(0x3C2,0x02 | (color?1:0));
	}

	vga_state.vga_base_3x0 = color ? 0x3D0 : 0x3B0;
}

