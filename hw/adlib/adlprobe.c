 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/adlib/adlib.h>

int probe_adlib(unsigned char sec) {
	unsigned char a,b,retry=3;
	unsigned short bas = sec ? 0x100 : 0;

	/* this code uses the 8254 for timing */
	if (!probe_8254())
		return 1;

	do {
		adlib_write(0x04+bas,0x60);			/* reset both timers */
		adlib_write(0x04+bas,0x80);			/* enable interrupts */
		a = adlib_status(sec);
		adlib_write(0x02+bas,0xFF);			/* timer 1 */
		adlib_write(0x04+bas,0x21);			/* start timer 1 */
		t8254_wait(t8254_us2ticks(100));
		b = adlib_status(sec);
		adlib_write(0x04+bas,0x60);			/* reset both timers */
		adlib_write(0x04+bas,0x00);			/* disable interrupts */

		if ((a&0xE0) == 0x00 && (b&0xE0) == 0xC0)
			return 1;

	} while (--retry != 0);

	return 0;
}

