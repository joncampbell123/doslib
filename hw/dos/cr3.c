/* cr3.c
 *
 * Test program: Attempt to read the CR3 register, see what happens
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
	uint32_t v_cr3=0;

	probe_dos();

	__asm {
		.386p
		int	3
		xor	eax,eax
		mov	eax,cr3
		mov	v_cr3,eax
	}

	printf("CR3=0x%08lX\n",(unsigned long)v_cr3);
	return 0;
}

