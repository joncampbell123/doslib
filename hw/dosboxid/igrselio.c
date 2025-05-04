
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

uint32_t dosbox_id_read_regsel() {
	uint32_t r;

	dosbox_id_reset_latch();

#if TARGET_MSDOS == 32
	r  = (uint32_t)inpd(DOSBOX_IDPORT(DOSBOX_ID_INDEX));
#else
	r  = (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX));
	r |= (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX)) << (uint32_t)16UL;
#endif

	return r;
}

void dosbox_id_write_regsel(const uint32_t reg) {
	dosbox_id_reset_latch();

#if TARGET_MSDOS == 32
	outpd(DOSBOX_IDPORT(DOSBOX_ID_INDEX),reg);
#else
	outpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX),(uint16_t)reg);
	outpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX),(uint16_t)(reg >> 16UL));
#endif
}

