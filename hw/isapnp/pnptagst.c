
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <dos/dos.h>
#include <isapnp/isapnp.h>

#include <hw/8254/8254.h>		/* 8254 timer */

static const char *isapnp_tag_strs[] = {
	/* small tags +0x00 */
	NULL,					/* 0x00 */
	"PnP version",
	"Logical device ID",			/* 0x02 */
	"Compatible device ID",
	"IRQ",					/* 0x04 */
	"DMA",
	"Dependent function start",		/* 0x06 */
	"Dependent function end",
	"I/O port",				/* 0x08 */
	"Fixed I/O port",
	NULL,					/* 0x0A */
	NULL,
	NULL,					/* 0x0C */
	NULL,
	"Vendor tag S-0xE",			/* 0x0E */
	"End",
	/* large tags +0x10 */
	NULL,					/* 0x10 */
	"Memory range",
	"Identifier string (ANSI)",		/* 0x12 */
	"Identifier string (Unicode)",
	"Vendor tag L-0x4",			/* 0x14 */
	"32-bit memory range",
	"32-bit fixed memory range"		/* 0x16 */
};

const char *isapnp_tag_str(uint8_t tag) { /* tag from struct isapnp_tag NOT the raw byte */
	if (tag >= (sizeof(isapnp_tag_strs)/sizeof(isapnp_tag_strs[0])))
		return NULL;

	return isapnp_tag_strs[tag];
}

