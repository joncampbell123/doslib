 
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
#else
unsigned short far *TRAM_C = (unsigned short far*)0xA0000000;
unsigned short far *TRAM_A = (unsigned short far*)0xA2000000;
#endif

static char hexes[] = "0123456789ABCDEF";

int main(int argc,char **argv) {
    unsigned short ch,chbase = 0;
    unsigned char chfill = 0;
    unsigned int x,y,o;
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

    for (y=0;y < 25;y++) printf("\n");
    printf("Hit ESC to exit.\n");

    do {
        /* what is what, on the side */
        for (y=0;y < 16;y++) {
            o = (0 * 80) + ((y + 3) * 2);
            TRAM_C[o] = hexes[y];
            TRAM_A[o] = 0xE1;

            ch = chbase + (y * 16U);
            o = ((y + 1) * 80) + (0 * 2);
            TRAM_C[o] = hexes[(ch>>12U)&0xFU];
            TRAM_A[o] = 0xE1;
            o++;
            TRAM_C[o] = hexes[(ch>>8U)&0xFU];
            TRAM_A[o] = 0xE1;
            o++;
            TRAM_C[o] = hexes[(ch>>4U)&0xFU];
            TRAM_A[o] = 0xE1;
            o++;
            TRAM_C[o] = hexes[(ch>>0U)&0xFU];
            TRAM_A[o] = 0xE1;
            o++;
        }

        /* draw directly on text VRAM a 16x16 grid of one character, each attribute */
        for (y=0;y < 16;y++) {
            for (x=0;x < 16;x++) {
                o = ((y + 1) * 80) + ((x + 3) * 2);
                ch = chbase + (y * 16) + x;
                TRAM_C[o+0] = ch;
                TRAM_C[o+1] = chfill ? ch : 0;
                TRAM_A[o+0] = 0xE1;
                TRAM_A[o+1] = 0xE1;
            }
        }

        /* keyboard input */
        c = getch();
        if (c == 27) break;
        else if (c == 'u') chbase -= 0x10;
        else if (c == 'd') chbase += 0x10;
        else if (c == 'U') chbase -= 0x100;
        else if (c == 'D') chbase += 0x100;
        else if (c == 'f') chfill ^= 1;
        else ch = c;
    } while (1);

	return 0;
}

