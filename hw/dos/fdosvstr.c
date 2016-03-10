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

#if TARGET_MSDOS == 32
char *freedos_kernel_version_str = NULL;
#else
char far *freedos_kernel_version_str = NULL;
#endif

// TODO: Need to re-test this against FreeDOS! Setup a VM!
#if TARGET_MSDOS == 32
const char *
#else
const char far *
#endif
dos_get_freedos_kernel_version_string() {
#if defined(TARGET_OS2)
	return NULL; // OS/2 does not run on FreeDOS
#elif defined(TARGET_WINDOWS)
	return NULL; // Windows does not run on FreeDOS (TODO: But maybe Windows 3.0 or Windows 3.1 can be tricked into running on FreeDOS?)
#else // MS-DOS
	union REGS regs;

	/* now retrieve the FreeDOS kernel string */
	/* FIXME: Does this syscall have a way to return an error or indicate that it didn't return a string? */
	regs.w.ax = 0x33FF;
# if TARGET_MSDOS == 32
	int386(0x21,&regs,&regs);
	return (const char*)(((uint32_t)regs.w.dx << 4UL) + (uint32_t)regs.w.ax);
# else
	int86(0x21,&regs,&regs);
	return (const char far*)MK_FP(regs.w.dx,regs.w.ax);
# endif
#endif
}

