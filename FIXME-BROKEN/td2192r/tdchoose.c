/* Traffic Dept 2192 choice menu.
 * (C) 2011 Jonathan Campbell.
 * Compile with Open Watcom */

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <ctype.h>

int main(int argc,char **argv) {
    int c;

    while (1) {
        printf("Which TD2192 episode do you want to play? (1-3, ESC=quit) ");
        fflush(stdout);
        c = getch();
        if (c == 0) c = getch() << 8;
        printf("\n");
        if (c == '1') {
            system("td1.exe");
        }
        else if (c == '2') {
            system("td2.exe");
        }
        else if (c == '3') {
            system("td3.exe");
        }
        else if (c == 27) { /* ESC */
            break;
        }
    }

    return 0;
}

