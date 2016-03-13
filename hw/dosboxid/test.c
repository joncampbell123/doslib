
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

static uint16_t dosbox_id_baseio = 0x28U;	// Default ports 0x28 - 0x2B

#define DOSBOX_IDPORT(x)	(dosbox_id_baseio+(x))

#define DOSBOX_ID_INDEX		(0U)
#define DOSBOX_ID_DATA		(1U)
#define DOSBOX_ID_STATUS	(2U)
#define DOSBOX_ID_COMMAND	(2U)

#define DOSBOX_ID_CMD_RESET_LATCH		(0x00U)
#define DOSBOX_ID_CMD_RESET_INTERFACE		(0xFFU)

#define DOSBOX_ID_RESET_DATA_CODE		(0xD05B0C5UL)

#define DOSBOX_ID_RESET_INDEX_CODE		(0xAA55BB66UL)

static inline void dosbox_id_reset_latch() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_RESET_LATCH);
}

static inline void dosbox_id_reset_interface() {
	outp(DOSBOX_IDPORT(DOSBOX_ID_COMMAND),DOSBOX_ID_CMD_RESET_INTERFACE);
}

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

uint32_t dosbox_id_read_data() {
	uint32_t r;

	dosbox_id_reset_latch();

#if TARGET_MSDOS == 32
	r  = (uint32_t)inpd(DOSBOX_IDPORT(DOSBOX_ID_DATA));
#else
	r  = (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_DATA));
	r |= (uint32_t)inpw(DOSBOX_IDPORT(DOSBOX_ID_DATA)) << (uint32_t)16UL;
#endif

	return r;
}

int probe_dosbox_id() {
	uint32_t t1,t2;

	dosbox_id_reset_interface();

	/* on reset, data should return DOSBOX_ID_RESET_DATA_CODE and index should return DOSBOX_ID_RESET_INDEX_CODE */
	t1 = dosbox_id_read_data();
	t2 = dosbox_id_read_regsel();
	if (t1 != DOSBOX_ID_RESET_DATA_CODE || t2 != DOSBOX_ID_RESET_INDEX_CODE) return 0;

	return 1;
}

int main(int argc,char **argv) {
	probe_dos();
	detect_windows();

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	return 0;
}

