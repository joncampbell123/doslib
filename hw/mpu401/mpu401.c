
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

#include <hw/mpu401/mpu401.h>

struct mpu401_ctx mpu401_card[MPU401_MAX_CARDS];

int init_mpu401() {
	memset(mpu401_card,0,sizeof(mpu401_card));
	return 1;
}

void free_mpu401() {
}

struct mpu401_ctx *mpu401_by_base(uint16_t x) {
	int i;

	for (i=0;i < MPU401_MAX_CARDS;i++) {
		if (mpu401_card[i].baseio == x)
			return mpu401_card+i;
	}

	return 0;
}

int mpu401_command(struct mpu401_ctx *cx,uint8_t d) {
	unsigned int patience = 100;

	do {
		if (inp(cx->baseio+MPU401_IO_STATUS) & 0x40) /* if not ready for cmd, wait and try again */
			t8254_wait(t8254_us2ticks(100));
		else {
			outp(cx->baseio+MPU401_IO_COMMAND,d);
			return 1;
		}
	} while (--patience != 0);
	return 0;
}

int mpu401_write(struct mpu401_ctx *cx,uint8_t d) {
	unsigned int patience = 100;

	do {
		if (inp(cx->baseio+MPU401_IO_STATUS) & 0x40) /* if not ready for cmd, wait and try again */
			t8254_wait(t8254_us2ticks(100));
		else {
			outp(cx->baseio+MPU401_IO_DATA,d);
			return 1;
		}
	} while (--patience != 0);
	return 0;
}

int mpu401_read(struct mpu401_ctx *cx) {
	unsigned int patience = 100;

	do {
		if (inp(cx->baseio+MPU401_IO_STATUS) & 0x80) /* if data ready not ready, wait and try again */
			t8254_wait(t8254_us2ticks(100));
		else {
			return inp(cx->baseio+MPU401_IO_DATA);
		}
	} while (--patience != 0);

	return -1;
}

/* this code makes sure the MPU exists */
int mpu401_probe(struct mpu401_ctx *cx) {
	unsigned int patience = 10;
	int c;

	if (cx->baseio == 0) return 0;

	/* check the command register. note however that if neither data is available
	 * or a command can be written this can return 0xFF */
	if (inp(cx->baseio+MPU401_IO_STATUS) == 0xFF) {
		/* hm, perhaps it's stuck returning data? */
		do { /* wait for it to signal no data and/or ability to write command */
			inp(cx->baseio+MPU401_IO_DATA);
			if (inp(cx->baseio+MPU401_IO_STATUS) != 0xFF)
				break;

			if (--patience == 0) return 0;
			t8254_wait(t8254_us2ticks(100)); /* 100us */
		} while(1);
	}

	patience=3;
	do {
		/* OK we got the status register to return something other than 0xFF.
		 * Issue a reset */
		if (mpu401_command(cx,0xFF)) {
			if ((c=mpu401_read(cx)) == 0xFE) {
				break;
			}
		}

		if (--patience == 0)
			return 0;

		t8254_wait(t8254_us2ticks(10)); /* 10us */
	} while (1);

	return 1;
}

/* this is for taking a base address and probing the I/O ports there to see if something like a SB DSP is there. */
/* it is STRONGLY recommended that you don't do this unless you try only 0x220 or 0x240 and you know that nothing
 * else important is there */
int mpu401_try_base(uint16_t iobase) {
	struct mpu401_ctx *cx;

	if ((iobase&0x3) != 0)
		return 0;
	if (iobase < 0x300 || iobase > 0x330)
		return 0;
	if (mpu401_by_base(iobase) != NULL)
		return 0;

	/* some of our detection relies on knowing what OS we're running under */
	cpu_probe();
	probe_dos();

	cx = mpu401_alloc_card();
	if (cx == NULL) return 0;

	cx->baseio = iobase;
	if (!mpu401_probe(cx)) {
		mpu401_free_card(cx);
		return 0;
	}

	return 1;
}

struct mpu401_ctx *mpu401_alloc_card() {
	int i;

	for (i=0;i < MPU401_MAX_CARDS;i++) {
		if (mpu401_card[i].baseio == 0)
			return mpu401_card+i;
	}

	return NULL;
}

void mpu401_free_card(struct mpu401_ctx *c) {
	memset(c,0,sizeof(*c));
}

