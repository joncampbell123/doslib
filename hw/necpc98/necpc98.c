 
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

struct nec_pc98_state_t		nec_pc98 = {0,0};

int probe_nec_pc98() {
	uint16_t avl;

	/* NEC PC-9800 series installation check.
	 * http://www.ctyme.com/intr/rb-2293.htm
	 *
	 * No additional information is provided, other than entering with AX = 0x1000 and AX != 0x1000 on return.
	 * I'm assuming some BIOSes will follow the pattern of AH=0x86 and CF=1 on this call, to be extra cautious
	 * against false positives.
	 *
	 * FIXME: Anyone over there in Japan still have these things around? All I have to test this code is the
	 *        Neko Project II emulator. It's a good bet I'll probably never get a hold of one here in the
	 *        United States. --J.C. */
	if (!nec_pc98.probed) {
		nec_pc98.probed = 1;
		avl = 0x1000;

		/* NTS: PC-98 is said to go as low as 8088, do not use PUSHA/POPA.
		 *      Clear carry flag in case BIOS sets carry flag to signal error. */
		__asm {
			push		ax
			push		bx
			push		cx
			push		dx
			clc
			mov		ax,0x1000
			int		0x1A
			jc		_ignore
			mov		avl,ax
_ignore:		pop		dx
			pop		cx
			pop		bx
			pop		ax
		}

		if (avl != 0x1000)
			nec_pc98.is_pc98 = 1;
	}
	
	return nec_pc98.is_pc98;
}

