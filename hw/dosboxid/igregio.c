
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

uint32_t dosbox_id_read_data_nrl() {
	uint32_t r;

#if TARGET_MSDOS == 32
	r  = (uint32_t)inpd(DOSBOX_IDPORT(DOSBOX_ID_DATA));
#else
	r  = (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_DATA));
	r |= (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_DATA)) << (uint32_t)16UL;
#endif

	return r;
}

uint32_t dosbox_id_read_data() {
	dosbox_id_reset_latch();
	return dosbox_id_read_data_nrl();
}

void dosbox_id_write_data_nrl(const uint32_t val) {
#if TARGET_MSDOS == 32
	outpd(DOSBOX_IDPORT(DOSBOX_ID_DATA),val);
#else
	outpw(DOSBOX_IDPORT(DOSBOX_ID_DATA),(uint16_t)val);
	outpw(DOSBOX_IDPORT(DOSBOX_ID_DATA),(uint16_t)(val >> 16UL));
#endif
}

void dosbox_id_write_data(const uint32_t val) {
	dosbox_id_reset_latch();
	dosbox_id_write_data_nrl(val);
}

