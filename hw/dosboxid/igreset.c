
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

int dosbox_id_reset() {
	uint32_t t1,t2;

	/* on reset, data should return DOSBOX_ID_RESET_DATA_CODE and index should return DOSBOX_ID_RESET_INDEX_CODE */
	dosbox_id_reset_interface();
	t1 = dosbox_id_read_data();
	t2 = dosbox_id_read_regsel();
	if (t1 != DOSBOX_ID_RESET_DATA_CODE || t2 != DOSBOX_ID_RESET_INDEX_CODE) return 0;
	return 1;
}

