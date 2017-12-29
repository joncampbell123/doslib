 
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

int main(int argc,char **argv) {
	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}

	printf(FULLWIDTH_ENGLISH "\n");
    printf(BOXES_STR "\n"); /* unfortunately the standard chars are invisible on real hardware */

    /* here's a string using NEC's proprietary code points to draw a box */
    printf("This box is visible\n");
    printf("\x86\x52\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x56\n");
    printf("\x86\x46  BOX  \x86\x46\n");
    printf("\x86\x5A\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x44\x86\x5E\n");

	return 0;
}

