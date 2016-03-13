
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

#define DOSBOX_ID_REG_IDENTIFY			(0x00000000UL)
#define DOSBOX_ID_REG_VERSION_STRING		(0x00000002UL)

/* return value of DOSBOX_ID_REG_IDENTIFY */
#define DOSBOX_ID_IDENTIFICATION		(0xD05B0740UL)

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

void dosbox_id_write_regsel(const uint32_t reg) {
	dosbox_id_reset_latch();

#if TARGET_MSDOS == 32
	outpd(DOSBOX_IDPORT(DOSBOX_ID_INDEX),reg);
#else
	outpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX),(uint16_t)reg);
	outpw(DOSBOX_IDPORT(DOSBOX_ID_INDEX),(uint16_t)(reg >> 16UL));
#endif
}

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

int dosbox_id_reset() {
	uint32_t t1,t2;

	/* on reset, data should return DOSBOX_ID_RESET_DATA_CODE and index should return DOSBOX_ID_RESET_INDEX_CODE */
	dosbox_id_reset_interface();
	t1 = dosbox_id_read_data();
	t2 = dosbox_id_read_regsel();
	if (t1 != DOSBOX_ID_RESET_DATA_CODE || t2 != DOSBOX_ID_RESET_INDEX_CODE) return 0;
	return 1;
}

uint32_t dosbox_id_read_identification() {
	/* now read the identify register */
	dosbox_id_write_regsel(DOSBOX_ID_REG_IDENTIFY);
	return dosbox_id_read_data();
}

int probe_dosbox_id() {
	uint32_t t;

	if (!dosbox_id_reset()) return 0;

	t = dosbox_id_read_identification();
	if (t != DOSBOX_ID_IDENTIFICATION) return 0;

	return 1;
}

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

	return 0;
}

