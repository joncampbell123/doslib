
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


static inline void rtc_io_finished() {
	outp(0x70,0x0D);
	inp(0x71);
}

static inline unsigned char rtc_io_read(unsigned char idx) {
	outp(0x70,idx | 0x80);	/* also mask off NMI */
	return inp(0x71);
}

static inline void rtc_io_write(unsigned char idx,unsigned char c) {
	outp(0x70,idx | 0x80);	/* also mask off NMI */
	outp(0x71,c);
}

static inline unsigned char rtc_io_read_unmasked(unsigned char idx) {
	outp(0x70,idx);
	return inp(0x71);
}

#define RTC_STATUS_C_INTERRUPT_REQUEST 0x80
#define RTC_STATUS_C_PERIODIC_INTERRUPT 0x40
#define RTC_STATUS_C_ALARM_INTERRUPT 0x20
#define RTC_STATUS_C_UPDATE_ENDED_INTERRUPT 0x10

int probe_rtc();
int rtc_wait_for_update_complete();

