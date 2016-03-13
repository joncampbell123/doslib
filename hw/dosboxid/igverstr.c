
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

int probe_dosbox_id_version_string(char *buf,size_t len) {
	uint32_t tmp;
	size_t i=0;

	if (len == 0) return 0;
	len = (len - 1U) & (~3U);
	if (len == 0) return 0;

	dosbox_id_write_regsel(DOSBOX_ID_REG_VERSION_STRING);
	dosbox_id_reset_latch();

	while ((i+4) < len) {
		tmp = dosbox_id_read_data_nrl();
		*((uint32_t*)(buf+i)) = tmp;
		i += 4;

		/* DOSBox-X will stop filling the register at the end of the string.
		 * The string "ABCDE" would be returned "ABCD" "E\0\0\0". The shortcut
		 * here is that we can test only the uppermost 8 bits for end of string
		 * instead of each byte. */
		if ((tmp >> 24UL) == 0) break;
	}

	assert(i < len);
	buf[i] = 0;
	return 1;
}

