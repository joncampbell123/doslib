
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
	uint32_t t,pt=0xAAAAAAAAUL;
	uint32_t tw,ptw=0xAAAAAAAAUL;

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

	while (1) {
		if (kbhit()) {
			if (getch() == 27)
				break;
		}

		dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR);
		t = dosbox_id_read_data();

		dosbox_id_write_regsel(DOSBOX_ID_CMD_GET_WINDOW_SIZE);
		tw = dosbox_id_read_data();

		if (t != pt || tw != ptw) {
			printf("\x0D" "%d,%d / %d,%d       ",
				(int)((int16_t)(t & 0xFFFFUL)),
				(int)((int16_t)(t >> 16)),
				(int)((int16_t)(tw & 0xFFFFUL)),
				(int)((int16_t)(tw >> 16)));
			fflush(stdout);
		}

		pt = t;
		ptw = tw;
	}
	printf("\n");

	return 0;
}

