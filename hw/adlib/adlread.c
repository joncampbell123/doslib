 
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

unsigned char adlib_read(unsigned short i) {
	unsigned char c;
#if defined(TARGET_PC98)
	outp(ADLIB_IO_INDEX+((i>>8)*0x200),(unsigned char)i);
#else
	outp(ADLIB_IO_INDEX+((i>>8)*2),(unsigned char)i);
#endif
	adlib_wait();
#if defined(TARGET_PC98)
	c = inp(ADLIB_IO_DATA+((i>>8)*0x200));
#else
	c = inp(ADLIB_IO_DATA+((i>>8)*2));
#endif
	adlib_wait();
	return c;
}

