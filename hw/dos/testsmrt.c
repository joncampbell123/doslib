/* testsmrt.c
 *
 * Test program: Demonstrate SMARTDRV detection code.
 * (C) 2014 Jonathan Campbell.
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
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>

int main() {
	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
		printf("    Flavor: '%s'\n",dos_flavor_str(dos_flavor));
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

	if (smartdrv_detect()) {
		printf("SMARTDRV %u.%02u detected!\n",smartdrv_version>>8,smartdrv_version&0xFF);
		printf("Now flushing\n");
		smartdrv_flush();
		printf("Made it.\n");
		smartdrv_close();
	}

	return 0;
}

