/* labelfcb.c
 *
 * Test program: Demonstrate reading the volume label using an FBC
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

static unsigned char fcb[44];
static unsigned char dta[128];
static unsigned char tmp[64];

int main(int argc,char **argv) {
	unsigned char stat=0;

	memset(fcb,0,sizeof(fcb));

	fcb[0x00] = 0xFF; // extended FCB
	fcb[0x06] = 0x08; // attribute
	fcb[0x07] = 0x00; // drive identifier (current drive)
	memset(fcb+0x08,'?',8); // filename
	memset(fcb+0x10,'?',3); // extension

	if (argc > 1) {
		const char *s = argv[1];
		unsigned int c;

		if (isalpha(s[0]) && s[1] == ':') {
			fcb[0x07] = toupper(s[0]) + 1 - 'A'; // drive identifier
			s += 2;
		}
		while (*s == '\\') s++;

		if (*s) {
			c=0;
			while (*s && *s != '\\' && *s != '.' && c < 8) fcb[0x08+(c++)] = *s++;
			while (c < 8) fcb[0x08+(c++)] = ' ';
			c=0;
			if (*s == '.') {
				s++;
				while (*s && *s != '\\' && *s != '.' && c < 3) fcb[0x10+(c++)] = *s++;
			}
			while (c < 3) fcb[0x10+(c++)] = ' ';
		}
	}

	printf("FCB (unopened) prior to INT 21h:\n");
	printf_fcb(fcb);

	memset(dta,0,sizeof(dta));

	__asm {
		push ds

		mov ax,seg dta
		mov ds,ax
		mov dx,offset dta
		mov ah,0x1A ; set DTA address (DS:DX)
		int 21h

		mov ax,seg fcb
		mov ds,ax
		mov dx,offset fcb
		mov ah,0x11 ; find first matching file (DS:DX)
		int 21h
		mov stat,al

		pop ds
	}

	printf("FCB in DTA after to INT 21h (AL=%02x aka status):\n",stat);
	printf_fcb(dta);

	if (stat == 0) {
		memcpy(tmp,dta+8,11); tmp[11] = 0;
		printf("Volume label is: '%s'\n",tmp);
	}
	else {
		printf("Volume label does not exist\n");
	}

	return 0;
}

