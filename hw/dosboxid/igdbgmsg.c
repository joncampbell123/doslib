
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

void dosbox_id_debug_message(const char *str) {
	unsigned char c;

	dosbox_id_write_regsel(DOSBOX_ID_REG_DEBUG_OUT);
	dosbox_id_reset_latch();
	while ((c=((unsigned char)(*str++))) != 0U) dosbox_id_write_data_nrl_u8(c);
	dosbox_id_flush_write();
}

