
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
	char tmp[128];

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

	if (probe_dosbox_id_version_string(tmp,sizeof(tmp)))
		printf("DOSBox version string: '%s'\n",tmp);
	else
		printf("DOSBox version string N/A\n");

    while (1) {
        uint32_t ft1616time;

        if (kbhit()) {
            int c = getch();
            if (c == 27) break;
        }

        /* NTS: Time is in milliseconds << 16.
         *      It will roll over every 65.536 seconds. */
		dosbox_id_write_regsel(DOSBOX_ID_CMD_READ_EMTIME);
		ft1616time = dosbox_id_read_data();

        printf("\x0D" "%.6f ",(double)ft1616time / (0x10000UL * 1000UL));
        fflush(stdout);
    }
    printf("\n");

	return 0;
}

