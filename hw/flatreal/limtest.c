/* Test program to demonstrate using the Flat Real Mode library.
 *
 * Interesting results:
 *    - Most modern CPUs do not seem to enforce segment limits in real mode.
 *      But anything prior to (about) a Pentium II will, especially if made by Intel.
 *    - Most emulators seem to not enforce real mode segment limits either.
 *      DOSBox and Virtual Box completely ignore limits. Bochs and Microsoft Virtual
 *      PC 2007 enforce segment limits.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <conio.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/flatreal/flatreal.h>

#if TARGET_MSDOS == 16
static const uint32_t maddrs[] = {
	FLATREALMODE_TEST,
	0xFFFFFFFF,
	0xFFFFFFFE,
	0xFFFFFFFD,
	0xFFFFFFFC,
	0xFFFFFFF0,
	0xFFFFF000,
	0xFFFFEFFF,
	0xFFFFEFFE,
	0xFFFFEFFD,
	0xFFFFEFFC,
	0xFFFFEFF0,
	0x00FFFFFF,
	0x00FFFFFE,
	0x00FFFFFD,
	0x00FFFFFC,
	0x00FFFFF0,
	0x0000FFFF,
	0x0000FFFE,
	0x0000FFFD,
	0x0000FFFC,
	0x0000FFF0,
	0x0000EFFF,
	0x0000EFFE,
	0x0000EFFD,
	0x0000EFFC,
	0x0000EFF0,
	0//end
};
#endif

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

    if (!flatrealmode_setup(FLATREALMODE_4GB)) {
        printf("Unable to set up Flat Real Mode\n");
	return 0;
    }

    {
	unsigned int row=1;
        unsigned int moa;
        unsigned int i;
	uint32_t maddr;

	printf("Whether or not a memory address faults given limits\n");
	printf("\n");

	for (moa=0;(maddr=maddrs[moa]) != (uint32_t)0ul;moa++) {
		printf("Read mem 0x%08lx: ",(unsigned long)maddr);

		flatrealmode_setup(FLATREALMODE_4GB); i = flatrealmode_testaddr(maddr);
		printf("4GB:%c ",i?'y':'n');

		flatrealmode_setup(FLATREALMODE_4GB - 0x1000); i = flatrealmode_testaddr(maddr);
		printf("4GB-4K:%c ",i?'y':'n');

		flatrealmode_setup(0x00FFFFFFul); i = flatrealmode_testaddr(maddr);
		printf("16MB:%c ",i?'y':'n');

		flatrealmode_setup(FLATREALMODE_64KB); i = flatrealmode_testaddr(maddr);
		printf("64KB:%c ",i?'y':'n');

		flatrealmode_setup(FLATREALMODE_64KB - 0x1000); i = flatrealmode_testaddr(maddr);
		printf("64KB-4K:%c ",i?'y':'n');

		printf("\n");

		flatrealmode_setup(FLATREALMODE_64KB);

		if ((++row) >= 24) {
			while (getch() != 13);
			row = 1;
		}
	}
    }

    return 0;
#else
    printf("Flat real mode is specific to the 16-bit real-mode builds of this suite\n");
    printf("and does not apply to 32-bit protected mode.\n");
    return 1;
#endif
}

