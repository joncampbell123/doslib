 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8250/8250.h>
#include <hw/dos/doswin.h>

const char *type_8250_parity(unsigned char parity) {
	if (parity & 1) {
		switch (parity >> 1) {
			case 0:	return "odd parity";
			case 1: return "even parity";
			case 2:	return "odd sticky parity";
			case 3: return "even sticky parity";
		};
	}

	return "no parity";
}

