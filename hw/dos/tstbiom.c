/* tstbiom.c
 *
 * Test program: BIOS extended memory layout and reporting
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
#include <hw/dos/doswin.h>
#include <hw/dos/biosmem.h>

int main() {
#if TARGET_MSDOS == 16
	cpu_probe();
	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

	{
		unsigned int l;
		if (biosmem_size_88(&l)) {
			printf("BIOS memory (INT 15H AH=0x88)\n");
			printf("    Total memory:        %uKB (%uMB)\n",l,l >> 10UL);
		}
	}

	{
		unsigned int l,h;
		if (biosmem_size_E801(&l,&h)) {
			printf("BIOS memory (INT 15H AX=0xE801)\n");
			printf("    Total memory:        1MB + %uKB + %luKB = %luKB (%luMB)\n",
				l,(unsigned long)h * 64UL,(unsigned long)l + ((unsigned long)h * 64UL) + 1024UL,
				((unsigned long)l + ((unsigned long)h * 64UL) + 1024UL) >> 10UL);
			if (l != 0) printf("    1-16MB:              0x%08lX-0x%08lX\n",0x100000UL,0x100000UL + ((unsigned long)l << 10UL) - 1UL);
			if (h != 0) printf("    16MB+:               0x%08lX-0x%08lX\n",0x1000000UL,0x1000000UL + ((unsigned long)h << 16UL) - 1UL);
		}
	}

	{
		struct bios_E820 nfo;
		unsigned long index,pindex;
		int len;

		index = pindex = 0;
		if ((len=biosmem_size_E820(&index,&nfo)) > 0) {
			printf("BIOS memory (INT 15H AX=E820)\n");
			do {
				printf("  len=%u index=0x%02lX 0x%012llx-0x%012llx type=0x%02lx attr=0x%02lx\n",
					len,(unsigned long)pindex,
					(unsigned long long)nfo.base,
					(unsigned long long)(nfo.base + nfo.length - 1ULL),
					nfo.type,nfo.ext_attributes);
				if (index == 0) break; /* the BIOS will return with EBX == 0 when returning the last item */
				pindex = index;
			} while ((len=biosmem_size_E820(&index,&nfo)) > 0);
		}
	}
#else /* == 32 */
	printf("This program is pointless in 32-bit protected mode\n");
#endif
	return 0;
}

