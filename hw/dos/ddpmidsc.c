/* dos.c
 *
 * Code to detect the surrounding DOS/Windows environment and support routines to work with it
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

/* TODO: Windows 3.1/95/98/ME have a DPMI server underneath.
 *       It would be neato at some point if these functions were
 *       available for use from Windows 3.1 Win16/Win32, and
 *       Windows 95/98/ME Win32 */
#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
void dpmi_free_descriptor(uint16_t d) {
	union REGS regs = {0};
	regs.w.ax = 0x0001;	/* DPMI free descriptor */
	regs.w.bx = d;
	int386(0x31,&regs,&regs);
}

uint16_t dpmi_alloc_descriptors(uint16_t c) {
	union REGS regs = {0};
	regs.w.ax = 0x0000;	/* allocate descriptor */
	regs.w.cx = 1;		/* just one */
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return regs.w.ax;
}

unsigned int dpmi_set_segment_base(uint16_t sel,uint32_t base) {
	union REGS regs = {0};
	regs.w.ax = 0x0007;	/* set segment base */
	regs.w.bx = sel;
	regs.w.cx = base >> 16UL;
	regs.w.dx = base;
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return 1;
}

unsigned int dpmi_set_segment_limit(uint16_t sel,uint32_t limit) {
	union REGS regs = {0};
	regs.w.ax = 0x0008;	/* set segment limit */
	regs.w.bx = sel;
	regs.w.cx = limit >> 16UL;
	regs.w.dx = limit;
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return 1;
}

unsigned int dpmi_set_segment_access(uint16_t sel,uint16_t access) {
	union REGS regs = {0};
	unsigned char c=0;

	/* the DPL/CPL value we give to the DPMI function below must match our privilege level, so
	 * get that value from our own selector */
	__asm {
		push	eax
		movzx	eax,sel
		and	al,3
		mov	c,al
		pop	eax
	}

	regs.w.ax = 0x0009;	/* set segment access rights */
	regs.w.bx = sel;
	regs.w.cx = (access & 0xFF9F) | (c << 5);	/* readable, code, CPL=same, present=1, 16-bit byte granular */
	int386(0x31,&regs,&regs);
	if (regs.w.cflag & 1) return 0;
	return 1;
}
#endif

