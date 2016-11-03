
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

#pragma pack(pop)

