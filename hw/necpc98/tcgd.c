 
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
#include <hw/8254/8254.h>
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
    unsigned int x,y,o;
    unsigned short ch;
    int c;

    /* NTS: Generally, if you write a doublewide character, the code is taken
     *      from that cell and continued over the next cell. The next cell
     *      contents are ignored. Except... that doesn't seem to be strictly
     *      true for ALL codes. Some codes are fullwidth but will show only
     *      the first half unless you write the same code twice. Some codes
     *      you think would make a doublewide code but don't. I'm not sure why.
     *
     *      If you see half of a character, try hitting 'f' to toggle filling
     *      both cells with the same code. */

	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}
    if (!probe_8254()) {
        return 1;
    }

    write_8254_system_timer(0);

    /* turn on the graphics layer */
    __asm {
        mov     ah,0x42         ; display area setup
        mov     ch,0xC0         ; 640x400 8-color graphics
        int     18h
    }

    ch = 'A';
    for (y=0;y < 25;y++) printf("\n");
    printf("Hit ESC to exit.\n");

    /* main loop */
#if TARGET_MSDOS == 32
    memset(GRAM_G,0,0x8000);
    memset(GRAM_R,0,0x8000);
    memset(GRAM_B,0,0x8000);
#else
    _fmemset(GRAM_G,0,0x8000);
    _fmemset(GRAM_R,0,0x8000);
    _fmemset(GRAM_B,0,0x8000);
#endif

    do {
        TRAM_C[0] = ch;
        TRAM_A[0] = 0xE1;
        TRAM_C[1] = ch;
        TRAM_A[1] = 0xE1;

        for (x=0;x < 4;x++) {
            TRAM_C[3+x] = hexes[(ch >> (12 - (x * 4))) & 0xF];
            TRAM_A[3+x] = 0xC1;
        }

        for (y=0;y < 2;y++) {
            outp(0xA3,ch & 0xFF);
            outp(0xA1,ch >> 8);
            for (x=0;x < 0x10;x++) {
                outp(0xA5,x + ((1-y) << 5));
                bmp[x + (y << 4)] = inp(0xA9);
            }
        }

        for (y=0;y < 2;y++) {
            for (x=0;x < 0x10;x++) {
                o = 10 + y + (x * 80);
                GRAM_G[o] = bmp[x + (y << 4)];
                GRAM_R[o] = 0;
                GRAM_B[o] = 0;
            }
        }

        for (y=0;y < 16;y++) {
            for (x=0;x < 0x80;x++) {
                o = 13 + y + (x * 80);
                GRAM_G[o] = (bmp[(x >> 3) + ((y >> 3) << 4)] & (0x80 >> (y & 7))) ? 0xFF : 0x00;
                GRAM_R[o] = 0;
                GRAM_B[o] = 0;
            }
        }


        c = getch();
        if (c == 27) break;
        else if (c == 'a') {
            ch--;
        }
        else if (c == 's') {
            ch++;
        }
        else if (c == 'A') {
            ch -= 0x100;
        }
        else if (c == 'S') {
            ch += 0x100;
        }
    } while (1);

    /* turn off the graphics layer */
    __asm {
        mov     ah,0x41         ; hide graphics layer
        int     18h
    }

	return 0;
}

