 
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

unsigned char row[0x10 * 256];

int main(int argc,char **argv) {
    unsigned int x,y,r,o;
    FILE *fp;

	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}

    fp = fopen("anex86.bmp","wb");
    if (fp == NULL) return 1;

    memset(row,0,14UL + 40UL);

    /* bitmap file header */
    memcpy(row+0x00,"BM",2);
    *((uint32_t*)(row+0x02)) = 14UL + 40UL + 8UL + ((2048UL/8UL) * 2048UL);
    *((uint16_t*)(row+0x06)) = 0;
    *((uint16_t*)(row+0x08)) = 0;
    *((uint32_t*)(row+0x0A)) = 14UL + 40UL + 8UL;

    /* bitmap header */
    *((uint32_t*)(row+0x0E+0x00)) = 40;     // biSize
    *((uint32_t*)(row+0x0E+0x04)) = 2048;   // biWidth
    *((uint32_t*)(row+0x0E+0x08)) = 2048;   // biHeight
    *((uint16_t*)(row+0x0E+0x0C)) = 1;      // biPlanes
    *((uint16_t*)(row+0x0E+0x0E)) = 1;      // biBitCount
    *((uint32_t*)(row+0x0E+0x10)) = 0;      // biCompression
    *((uint32_t*)(row+0x0E+0x14)) = ((2048UL/8UL) * 2048UL);// biSizeImage

    /* color palette */
    *((uint32_t*)(row+0x36)) = 0x000000;
    *((uint32_t*)(row+0x3A)) = 0xFFFFFF;

    /* write it */
    fwrite(row,14UL + 40UL + 8UL,1,fp);

    /* read off the character rows one by one.
     * because BMP files are inverted in the Y direction, read column forward, row backward */
    y=127;
    do {
        for (x=0;x < 128;x++) {
            outp(0xA3,x);
            outp(0xA1,y);

            o = x * 2;
            for (r=0;r < 16;r++) {
                outp(0xA5,r + 0x20);
                row[o + ((15 - r) << 8U) + 0] = inp(0xA9) ^ 0xFF;

                outp(0xA5,r + 0x00);
                row[o + ((15 - r) << 8U) + 1] = inp(0xA9) ^ 0xFF;
            }
        }

        fwrite(row,16 * 256,1,fp);

        if ((--y) == 0) break;
    } while (1);

    /* 0x00-0xFF are single-wide */
    {
        for (x=0;x < 256;x++) {
            outp(0xA3,x);
            outp(0xA1,0);

            o = x;
            for (r=0;r < 16;r++) {
                outp(0xA5,r + 0x00);
                row[o + ((15 - r) << 8U)] = inp(0xA9) ^ 0xFF;
            }
        }

        fwrite(row,16 * 256,1,fp);
    }

    fclose(fp);
	return 0;
}

