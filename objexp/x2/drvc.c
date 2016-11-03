
/* does NOT use C runtime */

#include <conio.h>
#include <stdint.h>

#pragma pack(push,1)
struct dosdrv_header_t {
    unsigned char far*          next_dosdrv;        // +0x00 or 0xFFFF:0xFFFF if last
    uint16_t                    flags;              // +0x04
    uint16_t                    strategy_offset;    // +0x06 offset of strategy routine
    uint16_t                    interrupt_offset;   // +0x08 offset of interrupt routine
    char                        name[8];            // +0x0A device name
};                                                  // =0x12 total

#define dosdrv_header_flags_CHARDEV                     (1U << 15U) // if set, character device (clear if block device)
#define dosdrv_header_flags_GENERIC_IOCTL               (1U << 6U)  // generic IOCTL functions supported
#define dosdrv_header_flags_IOCTL_QUERIES               (1U << 7U)  // IOCTL queries supported
#define dosdrv_header_flags_IOCTL_02_03                 (1U << 14U) // IOCTL functions 02h and 03h supported
// if character device
#define dosdrv_header_flags_CHR_STDIN_DEV               (1U << 0U)  // is standard input device
#define dosdrv_header_flags_CHR_STDOUT_DEV              (1U << 1U)  // is standard output device
#define dosdrv_header_flags_CHR_NULL_DEV                (1U << 2U)  // is standard NUL device
#define dosdrv_header_flags_CHR_CLOCK_DEV               (1U << 3U)  // is standard CLOCK device
#define dosdrv_header_flags_CHR_INT29_SUPPORTED         (1U << 4U)  // INT 29h fast character output supported
#define dosdrv_header_flags_CHR_OPEN_CLOSE              (1U << 11U) // open/close supported
#define dosdrv_header_flags_CHR_OUTPUT_UNTIL_BUSY       (1U << 13U) // output until busy supported
// if block device
#define dosdrv_header_flags_BLK_32BIT_SECTORS_SUPPORTED (1U << 1U)  // 32-bit sector addresses supported
#define dosdrv_header_flags_BLK_OPEN_CLOSE_MEDIA        (1U << 11U) // open/close removable media functions supported
#define dosdrv_header_flags_BLK_MEDIA_FAT_REQUIRED      (1U << 13U) // media descriptor byte of FAT required to determine media format

/* NTS: This is the BASE of the request structure. Additional fields appear depending on the request.
 *      Always check the request length! */
struct dosdrv_request_base_t {
    uint8_t                     request_length;     // +0x00 (from DOS)
    uint8_t                     unit_number;        // +0x01 (from DOS)
    uint8_t                     command;            // +0x02 (from DOS)
    uint16_t                    status;             // +0x03 (return to DOS)
};                                                  // =0x05

enum {
    dosdrv_request_command_INIT=0x00,               // BLK CHR 2.0+
    dosdrv_request_command_MEDIA_CHECK=0x01,        // BLK     2.0+
    dosdrv_request_command_BUILD_BPB=0x02,          // BLK     2.0+
    dosdrv_request_command_CTRL_INFO_READ=0x03,     // BLK CHR 2.0+
    dosdrv_request_command_READ=0x04,               // BLK CHR 2.0+
    dosdrv_request_command_ND_READ=0x05,            //     CHR 2.0+ non-destructive read
    dosdrv_request_command_INPUT_STATUS=0x06,       //     CHR 2.0+
    dosdrv_request_command_INPUT_FLUSH=0x07,        //     CHR 2.0+
    dosdrv_request_command_WRITE=0x08,              // BLK CHR 2.0+
    dosdrv_request_command_WRITE_WITH_VERIFY=0x09,  // BLK     2.0+
    dosdrv_request_command_OUTPUT_STATUS=0x0A,      //     CHR 2.0+
    dosdrv_request_command_OUTPUT_FLUSH=0x0B,       //     CHR 2.0+
    dosdrv_request_command_CTRL_INFO_WRITE=0x0C,    // BLK CHR 2.0+
    dosdrv_request_command_OPEN_DEVICE=0x0D,        // BLK CHR 3.0+
    dosdrv_request_command_CLOSE_DEVICE=0x0E,       // BLK CHR 3.0+
    dosdrv_request_command_REMOVABLE_MEDIA=0x0F,    // BLK     3.0+
    dosdrv_request_command_OUTPUT_UNTIL_BUSY=0x10,  //     CHR 3.0+
    dosdrv_request_command_GENERIC_IOCTL=0x13,      // BLK CHR 3.2+
    dosdrv_request_command_GET_LOGICAL_DEVICE=0x17, // BLK     3.2+
    dosdrv_request_command_SET_LOGICAL_DEVICE=0x18, // BLK     3,2+
    dosdrv_request_command_IOCTL_QUERY=0x19         // BLK CHR 5.0+
};

// request status field -------------------------------

// upper 8 bits of status are flags
#define dosdrv_request_status_flag_ERROR    (1U << 15U)
#define dosdrv_request_status_flag_BUSY     (1U <<  9U)
#define dosdrv_request_status_flag_DONE     (1U <<  8U)

// lower 8 bits are error code, if ERROR bit set
enum {
    dosdrv_request_status_code_WRITE_PROTECT_ERROR=0x00,
    dosdrv_request_status_code_UNKNOWN_UNIT=0x01,
    dosdrv_request_status_code_DRIVE_NOT_READY=0x02,
    dosdrv_request_status_code_UNKNOWN_COMMAND=0x03,
    dosdrv_request_status_code_CRC_ERROR=0x04,
    dosdrv_request_status_code_BAD_REQUEST_LENGTH=0x05,     // bad request structure length
    dosdrv_request_status_code_SEEK_ERROR=0x06,
    dosdrv_request_status_code_UNKNOWN_MEDIA=0x07,
    dosdrv_request_status_code_SECTOR_NOT_FOUND=0x08,
    dosdrv_request_status_code_OUT_OF_PAPER=0x09,
    dosdrv_request_status_code_WRITE_FAULT=0x0A,
    dosdrv_request_status_code_READ_FAULT=0x0B,
    dosdrv_request_status_code_GENERAL_FAILURE=0x0C,
    dosdrv_request_status_code_INVALID_DISK_CHANGE=0x0F
};
#pragma pack(pop)

extern struct dosdrv_header_t _cdecl far                dosdrv_header;
extern struct dosdrv_request_base_t far _cdecl * far    dosdrv_req_ptr;

/* NTS: __saveregs in Watcom C has no effect unless calling convention is watcall */
#define DOSDEVICE_INTERRUPT_PROC void __watcall __loadds __saveregs far

/* Interrupt procedure (from DOS). Must return via RETF. Must not have any parameters. Must load DS, save all regs.
 * All interaction with dos is through structure pointer given to strategy routine. */
DOSDEVICE_INTERRUPT_PROC dosdrv_interrupt(void) {
    if (dosdrv_req_ptr->request_length < sizeof(*dosdrv_req_ptr))
        goto not_long_enough;

    switch (dosdrv_req_ptr->command) {
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

