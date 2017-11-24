 
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
#include <hw/necpc98/necpc98.h>

#include "isjp_cnv.h" // Shift-JIS converted "This is Japanese" string constant

#if TARGET_MSDOS == 32
unsigned short *TRAM_C = (unsigned short*)0xA0000;
unsigned short *TRAM_A = (unsigned short*)0xA2000;
unsigned char  *GRAM_B = (unsigned char*) 0xA8000;
unsigned char  *GRAM_R = (unsigned char*) 0xB0000;
unsigned char  *GRAM_G = (unsigned char*) 0xB8000;
#else
unsigned short far *TRAM_C = (unsigned short far*)0xA0000000;
unsigned short far *TRAM_A = (unsigned short far*)0xA2000000;
unsigned char far  *GRAM_B = (unsigned char far*) 0xA8000000;
unsigned char far  *GRAM_R = (unsigned char far*) 0xB0000000;
unsigned char far  *GRAM_G = (unsigned char far*) 0xB8000000;
#endif

static char hexes[] = "0123456789ABCDEF";

unsigned char bmp[0x10 * 2];

int main(int argc,char **argv) {
	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}

	return 0;
}

