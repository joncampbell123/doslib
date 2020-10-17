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

int main() {
    int c,esc=0,frame=0;

    printf("I will show the input from STDIN while running.\n");
    printf("To exit to DOS, hit ESC three times.\n");

    // print raw data from STDIN.
    // to exit this loop, hit ESC three times.
    while (esc < 3) {
        if (kbhit()) {
            c = getch();

            if (c == 27) {
                printf("<ESC> ");
                esc++;
            }
            else if (c == -1) {
                printf("<-1> ");
            }
            else {
                esc=0;

                if (c >= 0x100)
                    printf("0x%04X ",(unsigned int)c);
                else
                    printf("0x%02X ",(unsigned int)c);
            }
        }
        else {
            printf(".%u." "\x08\x08\x08",frame);
            frame = (frame + 1) % 10;
        }

        fflush(stdout); /* no buffering, please */
    }

	return 0;
}

