
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
	unsigned int patience;
	char tmps[128];
	uint32_t tmp;

	(void)argc;
	(void)argv;
	(void)envp;

	probe_dos();
	detect_windows();

    if (windows_mode == WINDOWS_NT) {
        printf("This program is not compatible with Windows NT\n");
        return 1;
    }

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)
#else
	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}
#endif

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	if (probe_dosbox_id_version_string(tmps,sizeof(tmps)))
		printf("DOSBox version string: '%s'\n",tmps);
	else
		printf("DOSBox version string N/A\n");

	/* first check that the screenshot function is available */
	dosbox_id_write_regsel(DOSBOX_ID_REG_SCREENSHOT_TRIGGER);
	tmp = dosbox_id_read_data();
	if (tmp & DOSBOX_ID_SCREENSHOT_STATUS_NOT_AVAILABLE) {
		printf("DOSBox screenshot control register not available\n");
		return 1;
	}
	else if (tmp & DOSBOX_ID_SCREENSHOT_STATUS_NOT_ENABLED) {
		printf("DOSBox screenshot control register not enabled\n");
		return 1;
	}

	if (tmp & DOSBOX_ID_SCREENSHOT_STATUS_IMAGE_IN_PROGRESS)
		printf("WARNING: Screenshot already in progress\n");

	printf("Triggering DOSBox screenshot function\n");
	dosbox_id_debug_message("Triggering screenshot function\n");

	/* trigger it */
	dosbox_id_write_regsel(DOSBOX_ID_REG_SCREENSHOT_TRIGGER);
	dosbox_id_write_data(DOSBOX_ID_SCREENSHOT_IMAGE);

	/* did it trigger it? */
	dosbox_id_write_regsel(DOSBOX_ID_REG_SCREENSHOT_TRIGGER);
	tmp = dosbox_id_read_data();
	if (!(tmp & DOSBOX_ID_SCREENSHOT_STATUS_IMAGE_IN_PROGRESS)) {
		printf("DOSBox screenshot trigger failed.\n");
		return 1;
	}

	/* wait for the screenshot to happen.
	 * DOSBox will write the screenshot when VGA vertical retrace happens, which should be 1/70th of a second.
	 * So if it doesn't happen within 1 second, then it failed. */
#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)
	patience = 10; /* 1 second (10 x 100ms) (Windows 9x scheduler isn't THAT good!) */
#else
	patience = 1000; /* 1 second (1000 x 1ms) */
#endif
	do {
		if (--patience == 0) {
			printf("DOSBox failed to take screenshot (timeout)\n");
			return 1;
		}

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32 && !defined(WIN386)
		Sleep(100); /* 100ms */
#else
		t8254_wait(t8254_us2ticks(1000)); /* 1ms */
#endif

		dosbox_id_write_regsel(DOSBOX_ID_REG_SCREENSHOT_TRIGGER);
		tmp = dosbox_id_read_data();
	} while (tmp & DOSBOX_ID_SCREENSHOT_STATUS_IMAGE_IN_PROGRESS);

	printf("Screenshot done\n");
	return 0;
}

