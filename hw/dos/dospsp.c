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

int dos_parse_psp(uint16_t seg,struct dos_psp_cooked *e) {
	unsigned int i,o=0;

#if TARGET_MSDOS == 32
	e->raw = (unsigned char*)((uint32_t)seg << 4UL);
#else
	e->raw = (unsigned char FAR*)MK_FP(seg,0);
#endif
	e->memsize = *((uint16_t FAR*)(e->raw + 0x02));
	e->callpsp = *((uint16_t FAR*)(e->raw + 0x16));
	e->env = *((uint16_t FAR*)(e->raw + 0x2C));
	for (i=0;i < (unsigned char)e->raw[0x80] && e->raw[0x81+i] == ' ';) i++; /* why is there all this whitespace at the start? */
	for (;i < (unsigned char)e->raw[0x80];i++) e->cmd[o++] = e->raw[0x81+i];
	e->cmd[o] = 0;
	return 1;
}

