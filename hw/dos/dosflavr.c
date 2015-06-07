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

const char *windows_emulation_str(uint8_t e) {
	switch (e) {
		case WINEMU_NONE:	return "None";
		case WINEMU_WINE:	return "WINE (Wine Is Not an Emulator)";
		default:		break;
	}

	return "?";
}

const char *dos_flavor_str(uint8_t f) {
	switch (f) {
		case DOS_FLAVOR_NONE:		return "None";
		case DOS_FLAVOR_MSDOS:		return "MS-DOS";
		case DOS_FLAVOR_FREEDOS:	return "FreeDOS";
	}

	return "?";
}

