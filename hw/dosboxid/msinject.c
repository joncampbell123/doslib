
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

static inline uint32_t FIX88(const double n) {
	return (uint32_t)(n * 256);
}

int main(int argc,char **argv,char **envp) {
	int c,xd,yd;

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

	printf("Injecting mouse clicks.\n");
	while (!kbhit()) {
		dosbox_id_write_regsel(DOSBOX_ID_REG_8042_KB_INJECT);
		dosbox_id_write_data(DOSBOX_ID_8042_KB_INJECT_MOUSEBTN | /*press*/0x80 | /*button*/0);
		t8254_wait(t8254_us2ticks(100000));
		dosbox_id_write_regsel(DOSBOX_ID_REG_8042_KB_INJECT);
		dosbox_id_write_data(DOSBOX_ID_8042_KB_INJECT_MOUSEBTN | /*release*/0x00 | /*button*/0);
		t8254_wait(t8254_us2ticks(100000));
	}
	if (getch() == 27) return 1;

	printf("Injecting mouse movement.\n");
	c = 0;
	xd = 1;
	yd = 1;
	while (!kbhit()) {
		dosbox_id_write_regsel(DOSBOX_ID_REG_8042_KB_INJECT);
		dosbox_id_write_data(DOSBOX_ID_8042_KB_INJECT_MOUSEMX | (FIX88(0.5*xd + 1.0) << 16UL));
		dosbox_id_write_data(DOSBOX_ID_8042_KB_INJECT_MOUSEMY | (FIX88(0.5*yd + 1.0) << 16UL));
		t8254_wait(t8254_us2ticks(10000));

		if ((++c) >= 20) {
			if (xd == 1 && yd == 1)
				xd = -1;
			else if (xd == -1 && yd == 1)
				yd = -1;
			else if (xd == -1 && yd == -1)
				xd = 1;
			else if (xd == 1 && yd == -1)
				yd = 1;

			c -= 20;
		}
	}
	if (getch() == 27) return 1;

	return 0;
}

