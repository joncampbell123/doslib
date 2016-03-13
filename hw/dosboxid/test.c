
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

void dosbox_id_debug_message(const char *str) {
	unsigned char c;

	dosbox_id_write_regsel(DOSBOX_ID_REG_DEBUG_OUT);
	dosbox_id_reset_latch();
	while ((c=((unsigned char)(*str++))) != 0U) dosbox_id_write_data_nrl_u8(c);
	dosbox_id_flush_write();
}

int main(int argc,char **argv) {
	char tmp[128];

	probe_dos();
	detect_windows();

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	if (probe_dosbox_id_version_string(tmp,sizeof(tmp)))
		printf("DOSBox version string: '%s'\n",tmp);
	else
		printf("DOSBox version string N/A\n");

	dosbox_id_debug_message("This is a debug message\n");
	dosbox_id_debug_message("This is a multi-line debug message\n(second line here)\n");
	return 0;
}

