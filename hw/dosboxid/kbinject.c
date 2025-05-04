
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

/* <LSHIFT>H</LSHIFT>ello <LSHIFT>W</LSHIFT>orld */
unsigned char *msg_sc1 = "\x2A\x23\xA3\xAA\x12\x92\x26\xA6\x26\xA6\x18\x98\x39\xB9\x11\x91\x18\x98\x13\x93\x26\xA6\x20\xA0\x1C\x9C" "\x00";
unsigned char *msg_sc2 = "\x12""\x33""\xF0\x33""\xF0\x12""\x24\xF0\x24""\x4B\xF0\x4B""\x4B\xF0\x4B""\x44\xF0\x44""\x29\xF0\x29"
			 "\x12""\x1D""\xF0\x1D""\xF0\x12""\x44\xF0\x44""\x2D\xF0\x2D""\x4B\xF0\x4B""\x23\xF0\x23" "\x00";

int main(int argc,char **argv,char **envp) {
	unsigned char *msg_start,*msg;
	unsigned char scanset=0;
	unsigned char xlat=0;
	uint32_t kbstat;
	int c;

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

	dosbox_id_write_regsel(DOSBOX_ID_REG_8042_KB_STATUS);
	kbstat = dosbox_id_read_data();

	xlat = (kbstat & DOSBOX_ID_8042_KB_STATUS_CB_XLAT)?1:0;
	scanset = (unsigned char)((kbstat & DOSBOX_ID_8042_KB_STATUS_SCANSET_MASK) >> DOSBOX_ID_8042_KB_STATUS_SCANSET_SHIFT);
	printf("Keyboard state: scan code set %u, xlate=%u\n",scanset,xlat);

	if (scanset == 1 || (xlat && scanset == 2))
		msg_start = msg = msg_sc1;
	else if (scanset == 2)
		msg_start = msg = msg_sc2;
	else
		abort();

	while (1) {
		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27)
				break;

			if (c == 13) {
				printf("\n");
			}
			else if (c < 0x100) {
				printf("%c",(char)c);
				fflush(stdout);
			}
		}

		if (*msg == 0) msg = msg_start;
		if (*msg != 0) {
			dosbox_id_write_regsel(DOSBOX_ID_REG_8042_KB_INJECT);
			dosbox_id_write_data(DOSBOX_ID_8042_KB_INJECT_KB | ((unsigned char)(*msg++)));
		}

		t8254_wait(t8254_us2ticks(100000));
	}

	return 0;
}

