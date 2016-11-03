
/* does NOT use C runtime */

#include <i86.h>
#include <conio.h>
#include <stdint.h>

#include "drvhead.h"
#include "drvreq.h"
#include "drvvar.h"

static const char hello_world[] = "Hello!\r\n$"; // iNT 21h strings are $ terminated

void INIT_func(void) {
// NTS: Watcom C is missing an optimization opportunity here, so define instead
#define initreq ((struct dosdrv_request_init_t far*)dosdrv_req_ptr)

    __asm {
        push    ax
        push    ds
        push    dx
        mov     dx,seg hello_world
        mov     ds,dx
        mov     dx,offset hello_world
        mov     ah,0x09
        int     21h
        pop     dx
        pop     ds
        pop     ax
    }

    dosdrv_req_ptr->status = dosdrv_request_status_flag_DONE;

    /* DOS then expects us to tell it the end of our resident section.
     * This is why this code has _END offsets and init sections.
     * This init code and any INIT segments will be thrown away after we return. */
    initreq->end_of_driver = &dosdrv_initbegin;

#undef initreq
}

