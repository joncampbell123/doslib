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

int dos_device_next(struct dos_device_enum *e) {
	e->raw = e->next;
	if (e->raw == NULL || e->count >= 512 || e->no == 0xFFFFU)
		return 0;

	e->no = *((uint16_t FAR*)(e->raw + 0x0));
	e->ns = *((uint16_t FAR*)(e->raw + 0x2));
	e->attr = *((uint16_t FAR*)(e->raw + 0x04));
	e->entry = *((uint16_t FAR*)(e->raw + 0x06));
	e->intent = *((uint16_t FAR*)(e->raw + 0x08));
	if (!(e->attr & 0x8000)) {
		/* block device */
		_fmemcpy(e->name,e->raw+0x0B,8); e->name[7] = 0;
		if (e->name[0] < 33 || e->name[0] >= 127) e->name[0] = 0;
	}
	else {
		/* char device */
		_fmemcpy(e->name,e->raw+0x0A,8); e->name[8] = 0;
	}
	e->count++;

#if TARGET_MSDOS == 32
	e->next = (unsigned char*)((((uint32_t)(e->ns)) << 4UL) + e->no);
#else
	e->next = (unsigned char FAR*)MK_FP(e->ns,e->no);
#endif

	return 1;
}

int dos_device_first(struct dos_device_enum *e) {
	unsigned int offset = 0x22; /* most common, DOS 3.1 and later */

	if (dos_LOL == NULL)
		return 0;

	if (dos_version < 0x200) /* We don't know where the first device is in DOS 1.x */
		return 0;
	else if (dos_version < 0x300)
		offset = 0x17;
	else if (dos_version == 0x300)
		offset = 0x28;

	e->no = 0;
	e->ns = 0;
	e->count = 0;
	e->raw = e->next = dos_LOL + offset;
	if (_fmemcmp(e->raw+0xA,"NUL     ",8) != 0) /* should be the NUL device driver */
		return 0;

	return dos_device_next(e);
}

