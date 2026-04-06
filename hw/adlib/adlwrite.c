 
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

void adlib_write(unsigned short i,unsigned char d) {
#if defined(TARGET_PC98)
	outp(ADLIB_IO_INDEX+((i>>8)*0x200),(unsigned char)i);
#else
	outp(ADLIB_IO_INDEX+((i>>8)*2),(unsigned char)i);
#endif
	adlib_wait();
#if defined(TARGET_PC98)
	outp(ADLIB_IO_DATA+((i>>8)*0x200),d);
#else
	outp(ADLIB_IO_DATA+((i>>8)*2),d);
#endif
	adlib_wait();
}

