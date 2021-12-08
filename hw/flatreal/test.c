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

    if (flatrealmode_setup(FLATREALMODE_4GB)) {
        unsigned int i;
        unsigned char c;
        unsigned long d;
        unsigned short w;
	unsigned int count;

        printf("Flat real mode testing: will your CPU GP# fault if I access beyond 64KB?\n");

	printf("Reading 0x%08lx:\n\n",(unsigned long)FLATREALMODE_TEST);
	for (count=0;count < 2;count++) {
		flatrealmode_setup(FLATREALMODE_4GB); i = flatrealmode_test();
		printf("Processor with segment limits @ 4GB,         result: %d (%s)\n",i,i?"yes":"no");

		flatrealmode_setup(FLATREALMODE_4GB - 0x1000); i = flatrealmode_test();
		printf("Processor with segment limits @ 4GB-4KB,     result: %d (%s)\n",i,i?"yes":"no");

		flatrealmode_setup(0x00FFFFFF); i = flatrealmode_test();
		printf("Processor with segment limits @ 16MB,        result: %d (%s)\n",i,i?"yes":"no");

		flatrealmode_setup(FLATREALMODE_64KB); i = flatrealmode_test();
		printf("Processor with segment limits reset to 64KB, result: %d (%s)\n",i,i?"yes":"no");

		printf("\n");
	}

        while (getch() != 13);

        for (i=0;i < 0x40;i++) {
            c = flatrealmode_readb(0xB8000UL + (uint32_t)i);
            printf("%02X ",c);
        }
        printf("\n");
        for (i=0;i < 0x40;i++) {
            flatrealmode_writeb(0xB8000UL + (uint32_t)i,i);
        }
        while (getch() != 13);
        printf("\n");

        for (i=0;i < 0x40;i++) {
            w = flatrealmode_readw(0xB8000UL + (uint32_t)(i*2));
            printf("%04X ",w);
        }
        printf("\n");
        for (i=0;i < 0x40;i++) {
            flatrealmode_writew(0xB8000UL + (uint32_t)(i*2),i*0x101);
        }
        while (getch() != 13);
        printf("\n");

        for (i=0;i < 0x40;i++) {
            d = flatrealmode_readd(0xB8000UL + (uint32_t)(i*4));
            printf("%08lX ",(unsigned long)d);
        }
        printf("\n");
        for (i=0;i < 0x40;i++) {
            flatrealmode_writed(0xB8000UL + (uint32_t)(i*4),i*0x1010101UL);
        }
        while (getch() != 13);
    }
    else {
        printf("Unable to set up Flat Real Mode\n");
    }
    return 0;
#else
    printf("Flat real mode is specific to the 16-bit real-mode builds of this suite\n");
    printf("and does not apply to 32-bit protected mode.\n");
    return 1;
#endif
}

