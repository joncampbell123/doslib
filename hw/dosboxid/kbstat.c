
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
	uint32_t kbstat,pkbstat=0;

	(void)argc;
	(void)argv;
	(void)envp;

	probe_dos();
	detect_windows();

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	while (1) {
		if (kbhit()) {
			if (getch() == 27) break;
		}

		dosbox_id_write_regsel(DOSBOX_ID_REG_8042_KB_STATUS);
		kbstat = dosbox_id_read_data();

		if (kbstat^pkbstat) {
			pkbstat = kbstat;
			printf("Keyboard status: %08lx\n",(unsigned long)kbstat);

			printf("  LED=%02xh scanset=%u reset=%u active=%u scanning=%u auxactive=%u scheduled=%u\n",
				(unsigned char)((kbstat & DOSBOX_ID_8042_KB_STATUS_LED_STATE_MASK) >> DOSBOX_ID_8042_KB_STATUS_LED_STATE_SHIFT),
				(unsigned char)((kbstat & DOSBOX_ID_8042_KB_STATUS_SCANSET_MASK) >> DOSBOX_ID_8042_KB_STATUS_SCANSET_SHIFT),
				(kbstat & DOSBOX_ID_8042_KB_STATUS_RESET)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_ACTIVE)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_SCANNING)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_AUXACTIVE)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_SCHEDULED)?1:0);
			printf("  p60chg=%u auxchg=%u xlat=%u PS2lmr=%u%u%u ps2rep=%u ps2stream=%u lmr=%u%u%u\n",
				(kbstat & DOSBOX_ID_8042_KB_STATUS_P60CHANGED)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_AUXCHANGED)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_CB_XLAT)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_PS2_MOUSE_LBTN)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_PS2_MOUSE_MBTN)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_PS2_MOUSE_RBTN)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_MOUSE_REPORTING)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_MOUSE_STREAM_MODE)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_MOUSE_LBTN)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_MOUSE_MBTN)?1:0,
				(kbstat & DOSBOX_ID_8042_KB_STATUS_MOUSE_RBTN)?1:0);
		}
	}
	printf("\n");

	return 0;
}

