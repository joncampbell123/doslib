
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>

/* re-use a little code from the NE parser. */
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>
#include <hw/dos/exelehdr.h>
#include <hw/dos/exelepar.h>

const char *le_cpu_type_to_str(const uint8_t b) {
    switch (b) {
        case 0x01:  return "Intel 80286";
        case 0x02:  return "Intel 80386";
        case 0x03:  return "Intel 80486";
        case 0x04:  return "Intel 80586";
        case 0x20:  return "Intel i860 (N10)";
        case 0x21:  return "Intel N11";
        case 0x40:  return "MIPS Mark I (R2000/R3000)";
        case 0x41:  return "MIPS Mark II (R6000)";
        case 0x42:  return "MIPS Mark III (R4000)";
        default:    break;
    }

    return "";
}

const char *le_target_operating_system_to_str(const uint8_t b) {
    switch (b) {
        case 0x01:  return "OS/2";
        case 0x02:  return "Windows";
        case 0x03:  return "DOS 4.x";
        case 0x04:  return "Windows 386";
        default:    break;
    }

    return "";
}

