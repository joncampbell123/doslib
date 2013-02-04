
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/rtc/rtc.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC */

int probe_rtc() {
	unsigned char idx,match=0;

	_cli();
	for (idx=0;idx < 10;idx += 2) {
		if (rtc_io_read(idx) != 0xFF) match++;
	}
	rtc_io_read(0xC);
	rtc_io_read_unmasked(0xD);
	_sti();
	return (match >= 4);
}

int rtc_wait_for_update_complete() {
	int r=0,v;
	
	do {
		v=rtc_io_read(0xA);
		if (v & 0x80) r = 1; /* caller also needs to know if update in progress was seen */
	} while (v & 0x80);

	return r; /* return whether or not the update cycle was seen */
}

