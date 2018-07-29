
/* does NOT use C runtime */

#include <i86.h>
#include <conio.h>
#include <stdint.h>

#include <hw/dos/drvhead.h>
#include <hw/dos/drvreq.h>
#include <hw/dos/drvvar.h>

/* NTS: must declare this __based on _CODE so that it doesn't end up in _CONST.
 *      we want this string to be discarded along with INIT function after init.
 *      _CONST is not discarded after INIT. */
static const char __based( __segname("_CODE") ) hello_world[] = "Hello!\r\nThis is an example device driver that acts as a basic character device.\r\nYou can read and write me through the HELLO$ character device.\r\nHave fun!\r\n\r\n";

void INIT_func(void) {
// NTS: Watcom C is missing an optimization opportunity here, so define instead
#define initreq ((struct dosdrv_request_init_t far*)dosdrv_req_ptr)

    __asm {
        push    ax
        push    si

        ; NTS: We can refer to it without reloading DS because we tied everything to DGROUP in this project
        mov     si,offset hello_world

l1:     lodsb
        or      al,al
        jz      le
        mov     ah,0x0E
        int     10h
        jmp     l1
le:

        pop     si
        pop     ax
    }

    dosdrv_req_ptr->status = dosdrv_request_status_flag_DONE;

    /* DOS then expects us to tell it the end of our resident section.
     * This is why this code has _END offsets and init sections.
     * This init code and any INIT segments will be thrown away after we return. */
    initreq->end_of_driver = &dosdrv_initbegin;

#undef initreq
}

