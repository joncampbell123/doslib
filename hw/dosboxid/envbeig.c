
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
#include <hw/8237/8237.h>		/* 8237 timer */
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
	int en = 1;

	(void)argc;
	(void)argv;
	(void)envp;

	if (argc < 2) {
		printf("ENVBEIG <1|0>\n");
		printf("1=enable 0=disable VESA BIOS interface (for machine=svga_dosbox)\n");
		return 1;
	}

	en = atoi(argv[1]);

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

	dosbox_id_reset_latch();
	dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_CTL);
	dosbox_id_write_data(en ? 0 : DOSBOX_ID_REG_VGAIG_CTL_VBEMODESET_DISABLE);

	return 0;
}

