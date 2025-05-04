
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

int probe_dosbox_id() {
	uint32_t t;

	if (!dosbox_id_reset()) return 0;

	t = dosbox_id_read_identification();
	if (t != DOSBOX_ID_IDENTIFICATION) return 0;

	return 1;
}

