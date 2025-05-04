
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
	uint32_t newval = 0;

	(void)argc;
	(void)argv;
	(void)envp;

	if (argc > 1)
		newval = (uint32_t)strtoul(argv[1],NULL,0);

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

	if (newval != 0ul)
		printf("Setting watchdog timer to %lums\n",(unsigned long)newval);
	else
		printf("Clearing watchdog timer\n");

	dosbox_id_write_regsel(DOSBOX_ID_REG_SET_WATCHDOG);
	dosbox_id_write_data(newval);

	return 0;
}

