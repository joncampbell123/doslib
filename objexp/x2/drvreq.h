
#include <stdint.h>

#pragma pack(push,1)
/* NTS: This is the BASE of the request structure. Additional fields appear depending on the request.
 *      Always check the request length! */
struct dosdrv_request_base_t {
    uint8_t                     request_length;     // +0x00 (from DOS)
    uint8_t                     unit_number;        // +0x01 (from DOS)
    uint8_t                     command;            // +0x02 (from DOS)
    uint16_t                    status;             // +0x03 (return to DOS)
};                                                  // =0x05

/* dosdrv_request_command_INIT */
struct dosdrv_request_init_t {
    struct dosdrv_request_base_t        base;               // +0x00 (request_base_t)
    uint8_t                             reserved[8];        // +0x05
    uint8_t                             number_of_units;    // +0x0D
    void far*                           end_of_driver;      // +0x0E End of driver address / return break address (return to DOS)
                                                            //       Your init routine fills in the memory address that DOS is supposed
                                                            //       to free memory at. Bytes at or after this point are freed.
    union {
        uint16_t far*                   bios_parameter_block_ptr_array; // +0x12 pointer to array of near pointers to BPBs
        char far*                       device_equ_value;       // +0x12 pointer to char just past '=' in config.sys device= string (if char device)
    };
    uint8_t                             first_unit_drive_number;// +0x16 block device first unit drive number (0=A, 1=B, etc)
};                                                          // =0x17

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

