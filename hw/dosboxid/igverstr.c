
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

int probe_dosbox_id_version_string(char *buf,size_t len) {
	unsigned char c;
	size_t i=0;

	if (len == 0) return 0;
	len = (len - 1U) & (~3U);
	if (len == 0) return 0;

	dosbox_id_write_regsel(DOSBOX_ID_REG_VERSION_STRING);
	dosbox_id_reset_latch();
	dosbox_id_write_data_nrl_u8(0); // reset string read ptr in DOSBox-X
	dosbox_id_flush_write();

	while ((i+1) < len) {
		c = dosbox_id_read_data_nrl_u8();
		if (c == 0) break;
		buf[i++] = c;
	}

	buf[i] = 0;
	return 1;
}

