 
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

unsigned short gdc_base;

unsigned char xdigit2val(int c) {
    if (c >= '0' && c <= '9')
        return (unsigned char)(c - '0');
    else if (c >= 'a' && c <= 'f')
        return (unsigned char)(c + 10 - 'a');
    else if (c >= 'A' && c <= 'F')
        return (unsigned char)(c + 10 - 'A');

    return 0;
}

void raw_entry(void) {
    unsigned char cmd;
    unsigned char parm[16];
    unsigned char pi=0;
    int c;

    do {
        printf("\x0D");
        printf("Raw entry: Type first command, followed by parameters.\n");
        printf("Then hit ENTER. ESC to return to main menu.\n");
        printf("CMD? "); fflush(stdout);
        c = getch();

        if (c == 27) break;
        if (!isxdigit(c)) continue;
        cmd = xdigit2val(c) << 4;

        c = getch();
        if (!isxdigit(c)) continue;
        cmd += xdigit2val(c);

        printf("%02X <P> ",cmd); fflush(stdout);

        pi = 0;
        do {
            c = getch();
            if (!isxdigit(c)) break;
            parm[pi] = xdigit2val(c) << 4;

            c = getch();
            if (!isxdigit(c)) break;
            parm[pi] += xdigit2val(c);

            printf("%02X ",parm[pi]); fflush(stdout);
            if ((++pi) >= 16) break;
        } while (1);
        if (c == 27) break;

        printf("\n");

        if (c == 13 || c == 10) { // enter
            unsigned char i;

            _cli();

            while (inp(gdc_base) & 0x02); /* while FIFO full */
            outp(gdc_base+2,cmd); // write command

            for (i=0;i < pi;i++) {
                while (inp(gdc_base) & 0x02); /* while FIFO full */
                outp(gdc_base,parm[i]); // write param
            }

            _sti();
        }
    } while (1);

    printf("\n");
}

int main(int argc,char **argv) {
    int c;

	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}

    printf("Which GDC to talk to? (M=master at 0x60  S=slave at 0xA0)\n");
    c=tolower(getch());
    if (c == 'm')
        gdc_base = 0x60;
    else if (c == 's')
        gdc_base = 0xA0;
    else
        return 0;

    printf("With this program, you can manually enter commands and params\n");
    printf("to send to the GDC, or use predefined commands. Hit ESC to exit.\n");

    do {
        printf("r: RAW entry   ESC: exit\n");
        printf("? "); fflush(stdout);

        c=getch();
        printf("\n");
        if (c == 27) break;
        else if (c == 'r') raw_entry();
    } while (1);

	return 0;
}

