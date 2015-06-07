/* ntastrm.c
 *
 * Utility program: Manually trigger the removal of the DOSNTAST driver.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * If a program using DOSNTAST fails to unload the driver, it will remain resident.
 * This program allows you to remove it manually if that happens. */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/dosntvdm.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
	/* probe_dos() and detect_windows() should NOT auto-load the DOSNTAST driver.
	 * we're going to unload it if resident. */
	lib_dos_option.dont_load_dosntast=1;

	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);

	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%u\n",windows_version>>8,windows_version&0xFF);
		printf("    Method: '%s'\n",windows_version_method);
		if (windows_emulation != WINEMU_NONE)
			printf("    Emulation: '%s'\n",windows_emulation_str(windows_emulation));
		if (windows_emulation_comment_str != NULL)
			printf("    Emulation comment: '%s'\n",windows_emulation_comment_str);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

#if defined(NTVDM_CLIENT) && !defined(TARGET_WINDOWS)
	if (ntvdm_dosntast_handle != DOSNTAST_HANDLE_UNASSIGNED) {
		printf("DOSNTAST.VDD driver was loaded (handle=%u), unloading...\n",ntvdm_dosntast_handle);
		ntvdm_dosntast_unload();
	}
#endif

	return 0;
}

