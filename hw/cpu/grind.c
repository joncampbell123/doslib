/* grind.c
 *
 * Run instruction encodings through combinations and record changes to registers and flags.
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

#include <hw/cpu/cpu.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main() {
	cpu_probe();
	return 0;
}

