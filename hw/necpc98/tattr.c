 
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
    unsigned short ch = 'A';
    unsigned int x,y,o;
    int c;

	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}

    for (y=0;y < 25;y++) printf("\n");
    printf("Hit ESC to exit.\n");

    /* what is what, on the side */
    for (y=0;y < 16;y++) {
        o = (0 * 80) + ((y + 1) * 2);
        TRAM_C[o] = hexes[y];
        TRAM_A[o] = 0xE1;

        o = ((y + 1) * 80) + (0 * 2);
        TRAM_C[o] = hexes[y];
        TRAM_A[o] = 0xE1;
    }

    do {
        /* draw directly on text VRAM a 16x16 grid of one character, each attribute */
        for (y=0;y < 16;y++) {
            for (x=0;x < 16;x++) {
                o = ((y + 1) * 80) + ((x + 1) * 2);
                TRAM_C[o+0] = ch;
                TRAM_C[o+1] = ch;
                TRAM_A[o+0] = (y << 4) + x;
                TRAM_A[o+1] = (y << 4) + x;
            }
        }

        /* keyboard input */
        c = getch();
        if (c == 27) break;
        else if (c == ']') ch += 0x10;
        else if (c == '}') ch += 0x80;
        else if (c == '[') ch -= 0x10;
        else if (c == '{') ch -= 0x80;
        else if (c == ',') ch--;
        else if (c == '.') ch++;
        else ch = c;
    } while (1);

	return 0;
}

