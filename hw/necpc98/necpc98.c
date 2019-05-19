 
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
#if TARGET_MSDOS == 32
	unsigned char *bios_date = (unsigned char*)0xFFFF0UL; // Most DOS extenders map the low 1MB as-is
#else
	unsigned char far *bios_date = MK_FP(0xFFFFU,0x0000U);
#endif
	uint16_t avl;

	if (!nec_pc98.probed) {
		nec_pc98.probed = 1;
		nec_pc98.is_pc98 = 0;

	/* NEC PC-9800 series BIOS identification.
	 * https://web.archive.org/web/19970301075914/http://pc98.com/info/diff-at/diff-at-overvw.html
	 *
	 * AT compatible machines have a BIOS date in mm/dd/yy format (ASCII) at FFFF:0005, NEC PC-98 does not.
	 * Note that we do not assume PC-98 if we don't see those forward slashes, because I'm betting that many
	 * early IBM PC clones didn't do this either.
	 *
	 * Basically the test as documented is to see if the ASCII string at FFFF:0005 has forward slashes at
	 * memory addresses FFFF:7 and FFFF:A. My additional paranoia demands we check for the JMP FAR opcode
	 * as well. If we see forward slashes then this is IBM AT compatible hardware.
	 *
	 * Address              AT Compatible                PC-98
	 * -------------------------------------------------------
	 * FFFF:0000            JMP far                      JMP far
	 * FFFF:0005            ASCII date 'mm/dd/yy'        Not used
	 * FFFF:000D            Not used                     Not used
	 * FFFF:000E            Model ID                     ROM checksum
	 * FFFF:000F            Reserved                     ROM checksum
	 *
	 * Question: I assume the ROM checksum is a 16-bit sum to zero across the BIOS? From where to where? */
		if (bios_date[0x0] != 0xEA/*JMP FAR*/)
			return 0; // Nope. Both platforms would have a JMP FAR at FFFF:0000

		if (bios_date[0x7] == '/' && bios_date[0xA] == '/')
			return 0; // Nope. PC/AT

	/* NEC PC-9800 series installation check.
	 * http://www.ctyme.com/intr/rb-2293.htm
	 *
	 * No additional information is provided, other than entering with AX = 0x1000 and AX != 0x1000 on return.
	 * I'm assuming some BIOSes will follow the pattern of AH=0x86 and CF=1 on this call, to be extra cautious
	 * against false positives.
	 */
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

