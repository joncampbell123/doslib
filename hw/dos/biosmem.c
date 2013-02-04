/* biosmem.c
 *
 * Various BIOS INT 15h extended memory query functions.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/biosmem.h>

#if TARGET_MSDOS == 16
int biosmem_size_E820(unsigned long *index,struct bios_E820 *nfo) {
	if (cpu_basic_level < 3) /* requires a 386 or higher. If the programmer didn't call CPU detection that's OK he gets the crash he deserves */
		return 0;

	return _biosmem_size_E820_3(index,nfo);
}

int biosmem_size_88(unsigned int *sz) {
	union REGS regs={0};

	regs.x.ax = 0x8800;
	int86(0x15,&regs,&regs);
	if (regs.x.cflag & 1) /* CF=1 */
		return 0;
	if (regs.x.ax == 0)
		return 0;
	if (regs.h.ah == 0x86 || regs.h.ah == 0x80)
		return 0;

	*sz = regs.x.ax;
	return 1;
}

int biosmem_size_E801(unsigned int *low,unsigned int *high) {
	union REGS regs={0};

	regs.x.ax = 0xE801;
	int86(0x15,&regs,&regs);
	if (regs.x.cflag & 1) { /* CF=1 */
		return 0;
	}

	if (regs.x.ax == 0)
		regs.x.ax = regs.x.cx;
	else if (regs.x.cx == 0)
		regs.x.cx = regs.x.ax;

	if (regs.x.bx == 0)
		regs.x.bx = regs.x.dx;
	else if (regs.x.dx == 0)
		regs.x.dx = regs.x.bx;

	if (regs.x.ax != regs.x.cx || regs.x.bx != regs.x.dx)
		return 0;

	*low = regs.x.ax;
	*high = regs.x.bx;
	return 1;
}
#endif

