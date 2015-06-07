
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
#include <hw/dos/doswin.h>

int main(int argc,char **argv) {
	unsigned char tmp[128];
	unsigned int i;

	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_rtc()) {
		printf("RTC/CMOS not found\n");
		return 1;
	}
	probe_dos();
	detect_windows();

	_cli();
	for (i=0;i < 128;i++) tmp[i] = rtc_io_read(i);
	_sti();

	for (i=0;i < 128;i++) {
		if ((i&0xF) == 0) printf("%02X: ",i);
		if ((i&0xF) == 0x7) printf("%02X-",tmp[i]);
		else printf("%02X ",tmp[i]);
		if ((i&0xF) == 0xF) printf("\n");
	}

	rtc_io_finished();
	return 0;
}

