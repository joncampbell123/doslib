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

void vga_switch_to_mono() {
	unsigned char moc = 0;

	/* VGA: Easy, read the register, write it back */
	if (vga_flags & VGA_IS_VGA) {
		moc = inp(0x3CC);
		moc &= 0xFE; /* clear bit 0, respond to 0x3Bx */
		outp(0x3C2,moc);

		/* and then hack the graphics controller to remap the VRAM */
		moc = vga_read_GC(6);
		moc &= 0xF3;	/* clear bits 2-3 */
		moc |= 0x08;	/* bits 2-3 = 10 = B0000 */
		vga_write_GC(6,moc);
	}
	else if (vga_flags & VGA_IS_EGA) {
		/* EGA: We can't read it, but we can write it from our best guess */
		moc = 0x02;	/* VSYNC/HSYNC pos, low page odd/even, 25MHz clock, RAM enable */
		outp(0x3C2,moc);
		vga_write_GC(6,0x08|0x02); /* B0000 with odd/even */
	}
	else {
		/* whuh? */
		return;
	}

	/* next, tell the BIOS of the change */
#ifndef TARGET_WINDOWS
# if TARGET_MSDOS == 32
	*((unsigned char*)0x449) = 0x07; /* mode 7 */
	*((unsigned char*)0x463) = 0xB4;
	*((unsigned char*)0x410) |= 0x30; /*  -> change to 80x25 mono */
	*((unsigned char*)0x465) &= ~0x04; /* monochrome operation */
# else
	*((unsigned char far*)MK_FP(0x40,0x49)) = 0x07; /* mode 7 */
	*((unsigned char far*)MK_FP(0x40,0x63)) = 0xB4;
	*((unsigned char far*)MK_FP(0x40,0x10)) |= 0x30; /*  -> change to 80x25 mono */
	*((unsigned char far*)MK_FP(0x40,0x65)) &= ~0x04; /* monochrome operation */
# endif
#endif
}

