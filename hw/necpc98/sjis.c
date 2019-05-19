 
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

unsigned int pc98_sjis_dec1(struct ShiftJISDecoder *j,const unsigned char c) {
    if (c >= 0x81 && c <= 0x9F) {
        /* Reverse:
         *
         *    s1 = ((j1 + 1) / 2) + 112
         *    s1 - 112 = (j1 + 1) / 2
         *    (s1 - 112) * 2 = j1 + 1
         *    ((s1 - 112) * 2) - 1 = j1 */
        j->b1 = (c - 112) * 2;
        return 1;
    }
    else if (c >= 0xE0 && c <= 0xEF) {
        /* Reverse:
         *
         *    s1 = ((j1 + 1) / 2) + 176
         *    s1 - 176 = (j1 + 1) / 2
         *    (s1 - 176) * 2 = j1 + 1
         *    ((s1 - 176) * 2) - 1 = j1 */
        j->b1 = (c - 176) * 2;
        return 1;
    }

    return 0;
}

unsigned int pc98_sjis_dec2(struct ShiftJISDecoder *j,const unsigned char c) {
    /* assume dec1() returned true, else the caller shouldn't call this function */
    if (c >= 0x9F) { /* j1 is even */
        j->b2 = c - 126;
        return 1;
    }
    else if (c >= 0x40 && c != 0x7F) { /* j1 is odd */
        j->b1--; /* (j1 + 1) / 2 */
        j->b2 = c - 31;
        if (c >= 0x80) j->b2--; /* gap at 0x7F */
        return 1;
    }

    return 0;
}

