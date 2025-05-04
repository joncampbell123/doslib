
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
	uint32_t tmp,tmp2;
	char tmps[128];

	(void)argc;
	(void)argv;
	(void)envp;

	probe_dos();
	detect_windows();

    if (windows_mode == WINDOWS_NT) {
        printf("This program is not compatible with Windows NT\n");
        return 1;
    }

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
	tmp = tmp2 = dosbox_id_read_data();
	if (tmp & DOSBOX_ID_SCREENSHOT_STATUS_NOT_AVAILABLE) {
		printf("DOSBox screenshot control register not available\n");
		return 1;
	}
	else if (tmp & DOSBOX_ID_SCREENSHOT_STATUS_NOT_ENABLED) {
		printf("DOSBox screenshot control register not enabled\n");
		return 1;
	}

	/* trigger it */
	dosbox_id_write_regsel(DOSBOX_ID_REG_SCREENSHOT_TRIGGER);
	dosbox_id_write_data(DOSBOX_ID_SCREENSHOT_WAVE);

	/* make sure it toggled */
	dosbox_id_write_regsel(DOSBOX_ID_REG_SCREENSHOT_TRIGGER);
	tmp = dosbox_id_read_data();
	if (((tmp^tmp2) & DOSBOX_ID_SCREENSHOT_STATUS_WAVE_IN_PROGRESS) == 0) {
		printf("DOSBox wave trigger failed.\n");
		return 1;
	}

	if (tmp & DOSBOX_ID_SCREENSHOT_STATUS_WAVE_IN_PROGRESS)
		printf("DOSBox wave capture is running\n");
	else
		printf("DOSBox wave capture stopped\n");

	return 0;
}

