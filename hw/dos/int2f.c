
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
#include <hw/dos/doswin.h>
#include <hw/dos/dosntvdm.h>

/* support function: call THIS function first before attempting to call INT 2Fh.
 * Old versions of MS-DOS (especially IBM PC DOS 2.x) may have left INT 2Fh uninitialized to NULL */
#if !defined(TARGET_WINDOWS) && !defined(WIN386) && !defined(TARGET_OS2)
int int2f_is_valid(void) {
#if TARGET_MSDOS == 32
    return *((uint32_t*)(0xBC)) != 0x00000000;
#else
    return *((uint32_t far*)MK_FP(0x00,0xBC)) != 0x00000000;
#endif
}
#endif

