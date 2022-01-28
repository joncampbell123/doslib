/* tstlp.c
 *
 * Test program: DOS memory map awareness library
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

#if TARGET_MSDOS == 32
uint16_t cpu_read_my_cs() {
	uint32_t r=0;

	/* NTS: Don't forget that in 32-bit mode, PUSH <sreg> pushes the 16-bit segment value zero extended to 32-bit!
	 *      However Open Watcom will encode "pop ax" as "pop 16 bits off stack to AX" so the "pop eax" is needed
	 *      to keep the stack proper. It didn't used to do that, but some change made it happen that way, so,
	 *      now we have to explicitly say "pop eax" (2022/01/28) */
	__asm {
		push	cs
		pop	eax
		mov	r,eax
	}

	return (uint16_t)r;
}
#endif

int main() {
#if TARGET_MSDOS == 32
	uint16_t sg;
	int c;
#endif

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

	if (detect_dosbox_emu())
		printf("I am also running under DOSBox\n");

#if TARGET_MSDOS == 16
	/* TODO: Someday the dos_ltp() code will work in 16-bit mode, for environments like EMM386.EXE v86 or from within
	 *       the 16-bit Windows 3.1 world */
	printf("This program is intended to test memory mapping under 32-bit protected mode and does not apply to 16-bit real mode\n");
#else
	if (!dos_ltp_probe()) {
		printf("DOS linear->physical translation probe failed\n");
		return 1;
	}

	sg = cpu_read_my_cs();
	printf("Results:\n");
	printf("                Paging: %u   (CPU paging enabled)\n",dos_ltp_info.paging);
	printf("             DOS remap: %u   (memory below 1MB is remapped, not 1:1)\n",dos_ltp_info.dos_remap);
	printf("     Should lock pages: %u   (Extender may page to disk or move pages)\n",dos_ltp_info.should_lock_pages);
	printf("           Can't xlate: %u   (No way to determine physical mem addr)\n",dos_ltp_info.cant_xlate);
	printf("             Using PAE: %u   (DOS extender is using PAE)\n",dos_ltp_info.using_pae);
	printf("         DMA DOS xlate: %u   (DMA is translated too, virtualized)\n",dos_ltp_info.dma_dos_xlate);
	printf("        use VCPI xlate: %u   (VCPI server can provide translation)\n",dos_ltp_info.vcpi_xlate);
	printf("                   CR0: 0x%08lx\n",dos_ltp_info.cr0);
	printf("                   CR3: 0x%08lx\n",dos_ltp_info.cr3);
	printf("                   CR4: 0x%08lx\n",dos_ltp_info.cr4);
	printf("                    CS: 0x%04x %s CPL=%u\n",(unsigned int)sg,sg & 4 ? "LDT" : "GDT",sg&3);

	while ((c=getch()) != 13) {
		if (c == 27) return 1;
	}

	{
		uint32_t l;
		uint64_t p;
		int line=0;

		printf("Map test (32MB):\n");
		for (l=0;l < (32UL << 20UL);l += 4096) {
			printf("0x%08lX: ",l); fflush(stdout);
			p = dos_linear_to_phys(l);
			if (p == DOS_LTP_FAILED) printf("N/A\n");
			else printf("0x%08llX\n",p);

			if (++line >= 24) {
				line -= 24;
				while ((c=getch()) != 13) {
					if (c == 27) return 1;
				}
			}
		}
	}
#endif

	return 0;
}

