
/* does NOT use C runtime */

#include <i86.h>
#include <conio.h>
#include <stdint.h>

#include <hw/dos/drvhead.h>
#include <hw/dos/drvreq.h>
#include <hw/dos/drvvar.h>

/* const data we return */
static const char ret_message[] = "Hello world. This is a test message.\r\nYou should see this on your console as-is, no mistakes.\r\nDOS read this from my driver routine.\r\n";

static uint16_t ret_message_pos = 0;

void INIT_func(void);
static void READ_func(void);
static void WRITE_func(void);
static void OUTPUT_UNTIL_BUSY_func(void);
static void write_out(const unsigned char c);

/* interrupt routine, via stub */
#if defined(TINYMODE)
DOSDEVICE_INTERRUPT_NEAR_PROC dosdrv_interrupt_near(void) {
#else
DOSDEVICE_INTERRUPT_FAR_PROC dosdrv_interrupt_far(void) {
#endif
    switch (dosdrv_req_ptr->command) {
        case dosdrv_request_command_INIT:
            if (dosdrv_req_ptr->request_length < dosdrv_request_init_t_minimum)
                goto not_long_enough;

            INIT_func(); // NTS: This code is discarded after successful init!
            break;
        case dosdrv_request_command_OPEN_DEVICE:
        case dosdrv_request_command_CLOSE_DEVICE:
            dosdrv_req_ptr->status = dosdrv_request_status_flag_DONE;
            ret_message_pos = 0;
            break;
        case dosdrv_request_command_READ:
            READ_func();
            break;
        case dosdrv_request_command_WRITE:
        case dosdrv_request_command_WRITE_WITH_VERIFY:
            WRITE_func();
            break;
        case dosdrv_request_command_OUTPUT_UNTIL_BUSY:
            OUTPUT_UNTIL_BUSY_func();
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

static void READ_func(void) {
#define readreq ((struct dosdrv_request_read_t far*)dosdrv_req_ptr)
    unsigned short got = 0;
    unsigned short wanted = readreq->byte_count;
    unsigned char far *w = (unsigned char far*)(readreq->buffer_address);

    /* default OK */
    dosdrv_req_ptr->status = dosdrv_request_status_flag_DONE;

    /* read and copy */
    while (got < wanted && ret_message_pos < sizeof(ret_message)) {
        *w++ = ret_message[ret_message_pos++];
        got++;
    }

    /* return how much we read */
    readreq->byte_count = got;
#undef readreq
}

static void WRITE_func(void) {
#define writereq ((struct dosdrv_request_write_t far*)dosdrv_req_ptr) 
    unsigned short got = 0;
    unsigned short wanted = writereq->byte_count;
    const unsigned char far *r = (const unsigned char far*)(writereq->buffer_address);

    /* default OK */
    dosdrv_req_ptr->status = dosdrv_request_status_flag_DONE;

    /* write to screen, to prove we are getting the right data */
    while (got < wanted) {
        write_out(*r++);
        got++;
    }

    /* return how much we wrote */
    writereq->byte_count = got;
#undef writereq
}

static void OUTPUT_UNTIL_BUSY_func(void) {
#define writereq ((struct dosdrv_request_output_until_busy_t far*)dosdrv_req_ptr) 
    unsigned short got = 0;
    unsigned short wanted = writereq->byte_count;
    const unsigned char far *r = (const unsigned char far*)(writereq->buffer_address);

    /* default OK */
    dosdrv_req_ptr->status = dosdrv_request_status_flag_DONE;

    /* write to screen, to prove we are getting the right data */
    while (got < wanted) {
        write_out(*r++);
        got++;
    }

    /* return how much we wrote */
    writereq->byte_count = got;
#undef writereq
}

static void write_out(const unsigned char c) {
    __asm {
        push    ax

        mov     ah,0x0E
        mov     al,c
        int     10h

        mov     ah,0x0E
        mov     al,'.'
        int     10h

        pop     ax
    }
}

