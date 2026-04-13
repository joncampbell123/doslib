/* labelfcb.c
 *
 * Test program: Demonstrate reading the volume label using INT 21h AH=4Eh
 * (C) 2026 Jonathan Campbell.
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
#include <ctype.h>
#include <dos.h>

#if defined(TARGET_WINDOWS) || defined(TARGET_OS2)
# error no
#endif

static void printf_fcb(unsigned char *fcb/*44*/) {
	unsigned int c;

	for (c=0;c < 44;c++) {
		if ((c&15) == 0) printf("  ");
		printf("%02x ",fcb[c]);
		if ((c&15) == 15) printf("\n");
	}
	printf("\n");
}

static const char *srch = "*.*";
static unsigned char dta[128];
static unsigned char tmp[64];

int main(int argc,char **argv) {
	unsigned int stat = 0xFFFF;
	const char far *fp;

	if (argc > 1) {
		const char *s = argv[1];

		if (isalpha(s[0]) && s[1] == ':') {
			memcpy(tmp,s,2); memcpy(tmp+2,"*.*",4);
			srch = tmp; s += 2;
		}

		if (*s) srch = s;
	}

	memset(dta,0,sizeof(dta));
	fp = srch;

	__asm {
		push ds

		mov ax,seg dta
		mov ds,ax
		mov dx,offset dta
		mov ah,0x1A ; set DTA address (DS:DX)
		int 21h

		lds dx,fp
		mov ah,0x4E ; find first matching file
		xor al,al
		mov cx,8
		int 21h
		jnc nerr
		mov stat,ax
nerr:

		pop ds
	}

	printf("Result in DTA after INT 21h:");
	if (stat != 0xFFFF) printf(" (error=%04x)",stat);
	printf("\n");
	printf_fcb(dta);

	if (stat == 0xFFFF) {
		memcpy(tmp,dta+0x1E,13);
		tmp[13] = 0;
		printf("Volume label is '%s'\n",tmp);
	}

	return 0;
}

