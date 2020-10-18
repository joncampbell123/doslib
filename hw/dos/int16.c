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

static unsigned char cmd = 0;

static unsigned int int16_getch(void) {
    unsigned char c=cmd;
    unsigned short r=0;

    __asm {
        mov     ah,c
        int     16h
        mov     r,ax
    }

    return r;
}

int main(int argc,char **argv,char **envp) {
    unsigned int c;
    int esc=0;

    if (argc > 1) {
        cmd = (unsigned int)strtol(argv[1],NULL,0);
        cmd &= 0x10u;
    }

    printf("I will show what INT 16h on IBM PC systems is returning.\n");
    printf("Hit ESC three times to exit to DOS.\n");
    printf("The upper 8 bits are the BIOS scan code, and the lower are\n");
    printf("the ASCII code.\n");
    printf("Using INT 16h AH=%02xh.\n",cmd);
    printf("Specify 0 or 0x10 on command line to change.\n");

    while (esc < 3) {
        c = int16_getch();

        if ((c&0xFF) == 27) esc++;
        else esc=0;

        printf("0x%04X ",(unsigned int)c);

        fflush(stdout);
    }

	return 0;
}

