/* testbext.c
 *
 * Test program: Use INT 15h to copy extended memory.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * This program shows that the API works, and also reveals whether or not
 * the BIOS API is limited to 16MB. */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/biosext.h>

int main() {
#if TARGET_MSDOS == 16
	{
		unsigned int i;
		unsigned long addr;
		unsigned char tmp[16];

		printf("Copying data out of extended memory via BIOS\n");
		for (addr=0xFFF80UL;addr < 0x100000UL;addr += sizeof(tmp)) {
			memset(tmp,0,sizeof(tmp));
			if (bios_extcopy(ptr2phys(tmp),addr,16)) {
				printf("Problem copying\n");
				break;
			}
			for (i=0;i < 16;i++) printf("%02x ",tmp[i]);
			for (i=0;i < 16;i++) printf("%c",tmp[i] >= 32 ? tmp[i] : ' ');
			printf("\n");
		}
		while (getch() != 13);

		printf("Copying data out of extended memory via BIOS (16MB higher)\n");
		for (addr=0x10FFF80UL;addr < 0x1100000UL;addr += sizeof(tmp)) {
			memset(tmp,0,sizeof(tmp));
			if (bios_extcopy(ptr2phys(tmp),addr,16)) {
				printf("Problem copying\n");
				break;
			}
			for (i=0;i < 16;i++) printf("%02x ",tmp[i]);
			for (i=0;i < 16;i++) printf("%c",tmp[i] >= 32 ? tmp[i] : ' ');
			printf("\n");
		}
		while (getch() != 13);
	}
#else
	printf("Test does not apply to 32-bit builds\n");
#endif

	return 0;
}

