
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>

#if TARGET_MSDOS == 16 && defined(TARGET_WINDOWS)
# include <windows.h>

int cpu_basic_probe_via_winflags() {
	const register DWORD f = GetWinFlags();

	if (f & (WF_ENHANCED|WF_PMODE)) {
		/* Windows is telling us we're in protected mode, so use it's flags instead */
		if (f & WF_CPU486) return 4;
		if (f & WF_CPU386) return 3;
		if (f & WF_CPU286) return 2;
	}

	return -1; /* It's OK to detect normally */
}
#endif

