
/* does NOT use C runtime */

#include <i86.h>
#include <conio.h>
#include <stdint.h>

#include "drvhead.h"
#include "drvreq.h"
#include "drvvar.h"

/* put something in BSS */
int bss_stuff;

void INIT_func(void);

/* Interrupt procedure (from DOS). Must return via RETF. Must not have any parameters. Must load DS, save all regs.
 * All interaction with dos is through structure pointer given to strategy routine. */
DOSDEVICE_INTERRUPT_PROC dosdrv_interrupt(void) {
    switch (dosdrv_req_ptr->command) {
        case dosdrv_request_command_INIT:
            if (dosdrv_req_ptr->request_length < sizeof(struct dosdrv_request_init_t))
                goto not_long_enough;

            INIT_func(); // NTS: This code is discarded after successful init!
            break;
        default:
            /* I don't understand your request */
            dosdrv_req_ptr->status = dosdrv_request_status_flag_ERROR | dosdrv_request_status_code_UNKNOWN_COMMAND;
            break;
    }

    return;
/* common error: request not long enough */
not_long_enough:
    dosdrv_req_ptr->status = dosdrv_request_status_flag_ERROR | dosdrv_request_status_code_BAD_REQUEST_LENGTH;
    return;
}

