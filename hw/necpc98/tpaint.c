 
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

#if TARGET_MSDOS == 32
unsigned short *TRAM_C = (unsigned short*)0xA0000;
unsigned short *TRAM_A = (unsigned short*)0xA2000;
#else
unsigned short far *TRAM_C = (unsigned short far*)0xA0000000;
unsigned short far *TRAM_A = (unsigned short far*)0xA2000000;
#endif

unsigned short gdc_base = 0x60;

static char tmp[256];

static inline void gdc_write_command(const unsigned char cmd) {
    while (inp(gdc_base) & 0x02); /* while FIFO full */
    outp(gdc_base+2,cmd); // write command
}

static inline void gdc_write_data(const unsigned char d) {
    while (inp(gdc_base) & 0x02); /* while FIFO full */
    outp(gdc_base,d); // write data
}

int main(int argc,char **argv) {
    uint16_t charcode = 0x2106; /* double-wide A */
    uint8_t attrcode = 0xE1; /* white */
    unsigned char rowheight = 16;
    unsigned char rows = 25;
    unsigned char simplegraphics = 0;
    unsigned int cur_x = 0,cur_y = 0;
    unsigned char running = 1;
    unsigned char redraw = 1;
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

    {
        unsigned char c = 16;

        __asm {
            mov     ah,0x0B
            int     18h
            mov     c,al
        }

        if (c & 1) {
            rowheight = 20;
            rows = 20;
        }
    }

    printf("\x1B[m"); /* normal attributes */
    printf("\x1B[J"); /* clear screen */
    printf("\x1B[1>h"); /* hide function row */
    printf("\x1B[5>h"); /* hide cursor (so we can directly control it) */
    fflush(stdout);

    while (running) {
        if (redraw) {
            sprintf(tmp,"X=%02u Y=%02u char=0x%04x attr=0x%02x sg=%u",cur_x,cur_y,charcode,attrcode,simplegraphics);
            {
                const unsigned int ofs = 80*(rows-1);
                unsigned int i = 0;
                char *s = tmp;

                for (;i < 78;i++) {
                    char c = *s;

                    TRAM_C[i+ofs] = (uint16_t)c;
                    TRAM_A[i+ofs] = 0xE5; /* white, invert */

                    if (c != 0) s++;
                }

                TRAM_C[78+ofs] = charcode; TRAM_A[78+ofs] = 0xE5;
                TRAM_C[79+ofs] = charcode; TRAM_A[79+ofs] = 0xE5;
            }

            {
                unsigned int addr = (cur_y * 80) + cur_x;

                gdc_write_command(0x49); /* cursor position */
                gdc_write_data(addr & 0xFF);
                gdc_write_data((addr >> 8) & 0xFF);
                gdc_write_data(0);

                gdc_write_command(0x4B); /* cursor setup */
                gdc_write_data(0x80 + (rowheight - 1)); /* visible, 16 lines */
            }

            redraw = 0;
        }

        c = getch();
        if (c == 27) break;

        if (c == 0x08/*left arrow*/) {
            if (cur_x > 0) {
                cur_x--;
                redraw = 1;
            }
        }
        else if (c == 0x0C/*right arrow*/) {
            if (cur_x < 79) {
                cur_x++;
                redraw = 1;
            }
        }
        else if (c == 0x0B/*up arrow*/) {
            if (cur_y > 0) {
                cur_y--;
                redraw = 1;
            }
        }
        else if (c == 0x0A/*down arrow*/) {
            if (cur_y < (rows-1-1)) {
                cur_y++;
                redraw = 1;
            }
        }
        else if (c == 'g') {
            simplegraphics ^= 1;
            redraw = 1;

            outp(0x68,simplegraphics ? 0x01 : 0x00);
        }
        else if (c == 'C' || c == 'c') {
            charcode += (c == 'C' ? 0x0100 : 0xFF00);
            redraw = 1;
        }
        else if (c == 'V' || c == 'v') {
            charcode += (c == 'V' ? 0x0001 : 0xFFFF);
            redraw = 1;
        }
        else if (c >= '1' && c <= '8') {
            attrcode ^= 1U << ((unsigned char)(c - '1'));
            redraw = 1;
        }
        else if (c == ' ') {
            unsigned int addr = (cur_y * 80) + cur_x;

            TRAM_C[addr] = charcode;
            TRAM_A[addr] = attrcode;
        }
        else if (c == 9) {
            unsigned int addr = (cur_y * 80) + cur_x;

            TRAM_C[addr] = 0;
        }
        else if (c == '!') { /* 20/25-line switch */
            unsigned char c = 0;

            __asm {
                mov     ah,0x0B         ; get CRT mode
                int     18h
                xor     al,0x01         ; toggle 20/25-line mode
                mov     c,al
                mov     ah,0x0A
                int     18h
            }

            rowheight = (c & 1) ? 20 : 16;
            rows = (c & 1) ? 20 : 25;
            redraw = 1;

            if (rows == 25) {
                /* need to clear rows 19-24 */
                unsigned int i = 80*19;
                unsigned int j = 80*25;

                while (i < j) {
                    TRAM_C[i] = 0;
                    TRAM_A[i] = 0xE1;
                    i++;
                }
            }
        }
    }

    __asm {
        mov     ah,0x0B         ; get CRT mode
        int     18h
        and     al,0xFE         ; 25-line mode
        mov     ah,0x0A
        int     18h
    }

    printf("\n");
    printf("\x1B[1>l"); /* show function row */
    printf("\x1B[5>l"); /* show cursor */
    fflush(stdout);

	return 0;
}

