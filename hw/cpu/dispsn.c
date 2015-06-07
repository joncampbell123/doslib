/* dispsn.c
 *
 * Runtime CPU detection library:
 * demonstration program on disabling the Pentium III serial number.
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
 * Demonstrates detecting the Pentium III serial number, and whether we can
 * disable it */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/cpu/cpup3sn.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
	int testval = -1;	/* TEST FOR BUGFIX: Failure to save registers during call */
	cpu_probe();
	assert(testval == -1);
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));
	if (cpu_v86_active)
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	if (!(cpu_flags & CPU_FLAG_CPUID)) {
		printf(" - Your CPU does not support CPUID, how could it support the PSN?\n");
		return 1;
	}
	if (!(cpu_cpuid_features.a.raw[2] & (1UL << 18UL))) {
		printf(" - CPU does not support PSN, or has already disabled it\n");
		return 1;
	}

	if (argc > 1 && !strcmp(argv[1],"doit")) {
#if defined(TARGET_WINDOWS)
		printf("I don't think the Windows kernel would ever let me turn off the Processor Serial Number.\n");
#else
		if (cpu_v86_active) {
			printf("Virtual 8086 mode is active, I cannot switch off the Processor Serial Number.\n");
		}
		else {
			printf("Disabling PSN... "); fflush(stdout);
			cpu_disable_serial();
			printf("OK\n");
		}
#endif
	}
	else {
		printf("To disable the processor serial number, type 'dispsn doit'\n");
	}

	return 0;
}

