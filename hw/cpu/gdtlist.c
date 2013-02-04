/* gdt_list.c
 *
 * Test program for gdt_enum.c library.
 * Dumps the contents of the GDT onto the screen, if possible.
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS
 *   - Windows 3.0/3.1/95/98/ME
 *   - Windows NT 3.1/3.51/4.0/2000/XP/Vista/7
 *   - OS/2 16-bit
 *   - OS/2 32-bit
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/cpu/gdt_enum.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
	struct cpu_gdtlib_entry ent;
	unsigned int i,prn=0;
	int c;

	if (!cpu_gdtlib_init()) {
		printf("Unable to init CPU GDT library\n");
		return 1;
	}
	if (!cpu_gdtlib_read_current_regs()) {
		printf("Cannot read current regs in any meaningful way\n");
		return 1;
	}

	printf("GDTR: Limit=0x%04x Base=0x%08lX   LDTR: 0x%04x  ",
		(unsigned int)cpu_gdtlib_gdtr.limit,
		(unsigned long)cpu_gdtlib_gdtr.base,
		(unsigned int)cpu_gdtlib_ldtr);

	/* for reference: print my code and data segment/selector values */
	{
		uint16_t v_cs=0,v_ds=0;

		__asm {
			mov	ax,cs
			mov	v_cs,ax

			mov	ax,ds
			mov	v_ds,ax
		}

		printf("CS=%04X DS=%04X\n",v_cs,v_ds);
	}

	prn = 2;
	for (i=0;i < cpu_gdtlib_gdt_entries(&cpu_gdtlib_gdtr);i++) {
		if (cpu_gdtlib_gdt_read_entry(&ent,i)) {
			if (!cpu_gdtlib_empty_gdt_entry(&ent)) {
				printf("%04X: Lim=%08lX Base=%08lX Acc=%02X G=%02X ",
					(unsigned int)i << 3,
					(unsigned long)ent.limit,
					(unsigned long)ent.base,
					ent.access,
					ent.granularity);
				if (cpu_gdtlib_entry_is_special(&ent)) {
					printf("P=%u Other-%u Type=%u PL=%u",
						(ent.access>>7)&1,
						(ent.granularity&0x40) ? 32 : 16,
						(ent.access&0xF),
						(ent.access>>5)&3);
				}
				else if (cpu_gdtlib_entry_is_executable(&ent)) {
					printf("P=%u CODE-%u C=%u Read=%u PL=%u",
						(ent.access>>7)&1,
						(ent.granularity&0x40) ? 32 : 16,
						ent.access&1,
						(ent.access>>1)&1,
						(ent.access>>5)&3);
				}
				else {
					printf("P=%u DATA-%u D=%u RW=%u PL=%u",
						(ent.access>>7)&1,
						(ent.granularity&0x40) ? 32 : 16,
						ent.access&1,
						(ent.access>>1)&1,
						(ent.access>>5)&3);
				}
				printf("\n");

				if (++prn >= 23) {
					prn = 0;
					do {
						c = getch();
						if (c == 27) return 1;
					} while (c != 13);
				}
			}
		}
	}

	if (cpu_gdtlib_prepare_to_read_ldt()) {
		printf("LDT: Base=%08lX Limit=%08lX\n",
				(unsigned long)cpu_gdtlib_ldt_ent.base,
				(unsigned long)cpu_gdtlib_ldt_ent.limit);

		for (i=0;i < cpu_gdtlib_ldt_entries(&cpu_gdtlib_ldt_ent);i++) {
			if (cpu_gdtlib_ldt_read_entry(&ent,i)) {
				if (!cpu_gdtlib_empty_ldt_entry(&ent)) {
					printf("%04X: Lim=%08lX Base=%08lX Acc=%02X G=%02X ",
						(unsigned int)(i << 3)+4,
						(unsigned long)ent.limit,
						(unsigned long)ent.base,
						ent.access,
						ent.granularity);
					if (cpu_gdtlib_entry_is_special(&ent)) {
						printf("P=%u Other-%u Type=%u PL=%u",
							(ent.access>>7)&1,
							(ent.granularity&0x40) ? 32 : 16,
							(ent.access&0xF),
							(ent.access>>5)&3);
					}
					else if (cpu_gdtlib_entry_is_executable(&ent)) {
						printf("P=%u CODE-%u C=%u Read=%u PL=%u",
							(ent.access>>7)&1,
							(ent.granularity&0x40) ? 32 : 16,
							ent.access&1,
							(ent.access>>1)&1,
							(ent.access>>5)&3);
					}
					else {
						printf("P=%u DATA-%u D=%u RW=%u PL=%u",
							(ent.access>>7)&1,
							(ent.granularity&0x40) ? 32 : 16,
							ent.access&1,
							(ent.access>>1)&1,
							(ent.access>>5)&3);
					}
					printf("\n");

					if (++prn >= 23) {
						prn = 0;
						do {
							c = getch();
							if (c == 27) return 1;
						} while (c != 13);
					}
				}
			}
		}
	}

	cpu_gdtlib_free();
	return 0;
}

