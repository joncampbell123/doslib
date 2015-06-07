/* sseoff.c
 *
 * Test program: Detecting SSE extensions, and switching them off
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/cpu/cpusse.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
	cpu_probe();
	probe_dos();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));
	if (cpu_v86_active) {
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	}

	if (!cpu_check_sse_is_usable()) {
		printf("SSE not available to turn off.\n");
		printf("Reason: %s\n",cpu_sse_unusable_reason);
		printf("Detection method: %s\n",cpu_sse_usable_detection_method);
		printf("can turn off: %d\n",cpu_sse_usable_can_turn_off);
		printf("can turn on: %d\n",cpu_sse_usable_can_turn_on);
		return 1;
	}

	printf("SSE available, detection method: %s\n",cpu_sse_usable_detection_method);
	printf("can turn off: %d\n",cpu_sse_usable_can_turn_off);
	printf("can turn on: %d\n",cpu_sse_usable_can_turn_on);

	if (!cpu_sse_usable_can_turn_off) {
		printf("I can't turn SSE off, giving up\n");
		return 1;
	}

	if (!cpu_sse_disable()) {
		printf("Failed to turn off SSE\n");
		return 1;
	}

	printf("SSE is now disabled.\n");
	return 0;
}

