
/* does NOT use C runtime */

#include <conio.h>
#include <stdint.h>

extern unsigned char far _cdecl * far dosdrv_req_ptr;

/* NTS: __saveregs in Watcom C has no effect unless calling convention is watcall */
#define DOSDEVICE_INTERRUPT_PROC void __watcall __loadds __saveregs far

/* Interrupt procedure (from DOS). Must return via RETF. Must not have any parameters. Must load DS, save all regs.
 * All interaction with dos is through structure pointer given to strategy routine. */
DOSDEVICE_INTERRUPT_PROC dosdrv_interrupt(void) {
    if (dosdrv_req_ptr[0] == 0xAA) {
        dosdrv_req_ptr[1] = 0xBB;
    }
}

