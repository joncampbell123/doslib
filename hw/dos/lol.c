/* lol.c
 *
 * Test program: Make use of the MS-DOS "List of Lists" to print various things
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

/* FIXME: MS-DOS 6.22 under QEMU: This hangs, or causes QEMU to crash? */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>

int main() {
	int i,c,line = 0;
	unsigned char FAR *LOL;

	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
		printf("    Flavor: '%s'\n",dos_flavor_str(dos_flavor));
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

	if (detect_dosbox_emu())
		printf("I am also running under DOSBox\n");

	if ((LOL = dos_list_of_lists()) != NULL) {
		printf("DOS List of Lists at ");
#if TARGET_MSDOS == 32
		printf("0x%08lX\n",(unsigned long)LOL);
#else
		printf("%04x:%04x\n",FP_SEG(LOL),FP_OFF(LOL));
#endif
	}
	else {
		printf("Unable to locate the DOS 'list of lists'\n");
		return 0;
	}

	/* ENTER for next, ESC to stop */
	while ((c=getch()) != 13) {
		if (c == 27) return 0;
	}

	/* list MCBs */
	{
		struct dos_psp_cooked pspnfo;
		struct dos_mcb_enum men;

		if (dos_mcb_first(&men)) {
			printf("Resident MCBs\n"); line++;
			do {
				mcb_filter_name(&men);

				printf("[%04x]: %02x PSP=%04x size=%04x %-8s  ",
					men.cur_segment,men.type,men.psp,men.size,men.name);
				for (i=0;i < 32;i++) {
					c = men.ptr[i];
					if (c >= 32 && c <= 126) printf("%c",c);
					else printf(".");
				}
				printf("\n");

				if (men.psp >= 0x80 && men.psp < 0xFFFFU && dos_parse_psp(men.psp,&pspnfo)) {
					printf("    PSP memsize=%04xh callpsp=%04xh env=%04xh command='%s'\n",
						pspnfo.memsize,pspnfo.callpsp,pspnfo.env,pspnfo.cmd);

					if (++line >= 20) {
						line -= 20;

						/* ENTER for next, ESC to stop */
						while ((c=getch()) != 13) {
							if (c == 27) return 0;
						}
					}
				}

				if (++line >= 20) {
					line -= 20;

					/* ENTER for next, ESC to stop */
					while ((c=getch()) != 13) {
						if (c == 27) return 0;
					}
				}
			} while (dos_mcb_next(&men));

		/* ENTER for next, ESC to stop */
		while ((c=getch()) != 13) {
			if (c == 27) return 0;
		}
		}
	}

	/* list devices */
	{
		struct dos_device_enum denu;

		if (dos_device_first(&denu)) {
		printf("Device drivers\n"); line++;
		do {
			printf("    ATTR=%04Xh entry=%04Xh int=%04Xh %s\n",denu.attr,denu.entry,denu.intent,denu.name);

			if (++line >= 20) {
				line -= 20;

				/* ENTER for next, ESC to stop */
				while ((c=getch()) != 13) {
					if (c == 27) return 0;
				}
			}
		} while (dos_device_next(&denu));

		/* ENTER for next, ESC to stop */
		while ((c=getch()) != 13) {
			if (c == 27) return 0;
		}
		}
	}

	return 0;
}

