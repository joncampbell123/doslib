 
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
	printf("NEC PC-98 doslib test program\n");
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}

	/* NTS: If you need to say anything in Japanese, you'll need to code the strings in Shift-JIS in your printf statements.
	 *      We programmers today are spoiled by UTF-8 and unicode :) --J.C */
	printf("Hardware is PC-98. Good!\n");
	return 0;
}

