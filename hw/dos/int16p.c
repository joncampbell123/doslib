/* test.c
 *
 * Test program: Various info about DOS
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>

static unsigned int int16_getch(void) {
    unsigned short r=0;

    __asm {
        xor     ah,ah
        int     16h
        mov     r,ax
    }

    return r;
}

static unsigned int int16_peek(void) {
    unsigned short r=0xFFFFu;

    __asm {
        mov     ah,1
        int     16h
        jz      l1
        mov     r,ax
l1:
    }

    return r;
}

int main(int argc,char **argv,char **envp) {
    unsigned int c,c2;
    int esc=0;

    printf("I will show what INT 16h on IBM PC systems is returning.\n");
    printf("Hit ESC three times to exit to DOS.\n");
    printf("The upper 8 bits are the BIOS scan code, and the lower are\n");
    printf("the ASCII code.\n");

    while (esc < 3) {
        c = int16_peek();
        if (c != 0xFFFFu) {
            c2 = int16_getch();

            if ((c&0xFF) == 27) esc++;
            else esc=0;

            if (c == c2)
                printf("%04xh ",(unsigned int)c);
            else
                printf("<p=%04Xh g=%04Xh> ",(unsigned int)c,(unsigned int)c2);

            fflush(stdout);
        }
    }

	return 0;
}

