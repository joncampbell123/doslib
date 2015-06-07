
#ifdef TARGET_WINDOWS
# include <windows.h>
#endif

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>
#include <malloc.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/tgusumid.h>

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
/* returns interrupt vector */
int gravis_ultramid_detect() {
    unsigned char FAR *ex;
    uint16_t s,o;
    int i = 0x78;

    while (i < 0x90) {
#if TARGET_MSDOS == 32
	o = *((uint16_t*)(i*4U));
	s = *((uint16_t*)((i*4U)+2U));
#else
	o = *((uint16_t far*)MK_FP(0,(uint16_t)i*4U));
	s = *((uint16_t far*)MK_FP(0,((uint16_t)i*4U)+2U));
#endif

	if (o == 0xFFFF || s == 0x0000 || s == 0xFFFF) {
	    i++;
	    continue;
	}

	/* we're looking for "ULTRAMID" signature */
#if TARGET_MSDOS == 32
	ex = (unsigned char*)((s << 4UL) + 0x103);
	if (memcmp(ex,"ULTRAMID",8) == 0) return i;
#else
	ex = MK_FP(s,0x103);
	if (_fmemcmp(ex,"ULTRAMID",8) == 0) return i;
#endif

	i++;
    }

    return -1;
}
#endif /* !defined(TARGET_WINDOWS) */

