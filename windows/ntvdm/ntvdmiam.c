
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <malloc.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>
#include <i86.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/dos/dosbox.h>
#include <windows/ntvdm/ntvdmlib.h>

#if defined(NTVDM_CLIENT)
int i_am_ntvdm_client() {
	probe_dos();
	if (cpu_basic_level < 0) cpu_probe();
	detect_windows();

#if defined(TARGET_WINDOWS)
	/* WINE (Wine Is Not an Emulator) tends to run the code in
	 * a more direct fashion */
	if (windows_emulation == WINEMU_WINE)
		return 0;
#endif

	if (windows_mode == WINDOWS_NT)
		return 1;

	return 0;
}
#endif

