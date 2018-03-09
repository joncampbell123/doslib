 
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

int main(int argc,char **argv) {
    char tmp[128];
    int c,esc=0;
    int tmpi=0;

    printf("\x1B[2J");

    printf("Type your input, it will be written back to the console.\n");
    printf("Type <ESC> and then the ANSI code.\n");
    printf("Hit ESC three times to exit.\n");

    while (1) {
        c = getch();
        if (c == 27) {
            if (++esc == 3) break;
        }
        else {
            esc = 0;
        }

        if (c >= 32 || c == 27) {
            if (tmpi < 127)
                tmp[tmpi++] = (char)c;
        }
        else if (c == 13 || c == 10) {
            if (tmpi != 0) {
                fwrite(tmp,tmpi,1,stdout);
                fflush(stdout);
                tmpi = 0;
            }
        }
    }

    printf("\n");

	return 0;
}

