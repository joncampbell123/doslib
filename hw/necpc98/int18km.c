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

static unsigned char int18_getmap(unsigned char i) {
    unsigned char r=0;

    __asm {
        mov     ah,0x04
        mov     cl,i
        int     18h
        mov     r,ah
    }

    return r;
}

int main(int argc,char **argv,char **envp) {
    int esc=0;
    int c;

    printf("I will show what INT 18h on NEC PC-98 systems is returning\n");
    printf("for keymap queries. Hit ESC three times to exit to DOS.\n");

    while (esc < 3) {
        if (kbhit()) {
            c = getch();
            if (c == 27)
                esc++;
            else
                esc = 0;
        }

        printf("\x0D");
        for (c=0;c < 16;c++) printf("%02X ",int18_getmap((unsigned char)c));
        fflush(stdout);
    }

	return 0;
}

