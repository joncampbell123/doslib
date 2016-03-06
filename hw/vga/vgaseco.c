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

void vga_switch_to_color() {
	unsigned char moc = 0;

	/* VGA: Easy, read the register, write it back */
	if (vga_flags & VGA_IS_VGA) {
		moc = inp(0x3CC);
		moc |= 0x01; /* set bit 0, respond to 0x3Dx */
		outp(0x3C2,moc);

		/* and then hack the graphics controller to remap the VRAM */
		moc = vga_read_GC(6);
		moc &= 0xF3;	/* clear bits 2-3 */
		moc |= 0x0C;	/* bits 2-3 = 11 = B8000 */
		vga_write_GC(6,moc);
	}
	else if (vga_flags & VGA_IS_EGA) {
		/* EGA: We can't read it, but we can write it from our best guess */
		moc = 0x02|1;	/* VSYNC/HSYNC pos, low page odd/even, 25MHz clock, RAM enable */
		outp(0x3C2,moc);
		vga_write_GC(6,0x0C|0x02); /* B8000 with odd/even */
	}
	else {
		/* whuh? */
		return;
	}

	/* next, tell the BIOS of the change */
#ifndef TARGET_WINDOWS
# if TARGET_MSDOS == 32
	*((unsigned char*)0x449) = 0x03; /* mode 3 */
	*((unsigned char*)0x463) = 0xD4;
	*((unsigned char*)0x410) &= 0x30; /* INT 11 initial video mode */
	*((unsigned char*)0x410) |= 0x20; /*  -> change to 80x25 color */
	*((unsigned char*)0x465) |= 0x04; /* color operation */
# else
	*((unsigned char far*)MK_FP(0x40,0x49)) = 0x03; /* mode 3 */
	*((unsigned char far*)MK_FP(0x40,0x63)) = 0xD4;
	*((unsigned char far*)MK_FP(0x40,0x10)) &= 0x30; /* INT 11 initial video mode */
	*((unsigned char far*)MK_FP(0x40,0x10)) |= 0x20; /*  -> change to 80x25 color */
	*((unsigned char far*)MK_FP(0x40,0x65)) |= 0x04; /* color operation */
# endif
#endif
}

