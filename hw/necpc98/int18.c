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

static unsigned int int18_getch(void) {
    unsigned short r=0;

    __asm {
        xor     ah,ah
        int     18h
        mov     r,ax
    }

    return r;
}

int main(int argc,char **argv,char **envp) {
    unsigned int c;
    int esc=0;

    printf("I will show what INT 18h on NEC PC-98 systems is returning.\n");
    printf("Hit ESC three times to exit to DOS.\n");
    printf("The upper 8 bits are the scan code, and the lower are\n");
    printf("the ASCII code.\n");

    while (esc < 3) {
        c = int18_getch();

        if ((c&0xFF) == 27) esc++;
        else esc=0;

        printf("0x%04X ",(unsigned int)c);

        fflush(stdout);
    }

	return 0;
}

