
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

/* given the raw data of an RT_BITMAP, determine if it's an older Windows 1.x/2.x BITMAP
 * rather than a Windows 3.x BITMAPINFOHEADER */
unsigned int exe_ne_header_is_WINOLDBITMAP(const unsigned char *data/*at least 4 bytes*/,const size_t len) {
    if (len < 4)
        return 0;

    /* first byte is always 2
     * second byte usually zero, but values like 0x81 have been seen.
     * third-fourth bytes are always zero, and they correspond to the bmType field of the BITMAP structure */
    if (data[0] == 0x02 && (data[1] & (0xFF-0x81)) == 0x00 &&
        data[2] == 0x00 && data[3] == 0x00)
        return 1;

    return 0;
}

/* given the raw data of an RT_ICON, determine if it's an older Windows 1.x/2.x BITMAP format
 * rather than a Windows 3.x BITMAPINFOHEADER */
unsigned int exe_ne_header_is_WINOLDICON(const unsigned char *data/*at least 4 bytes*/,const size_t len) {
    if (len < 6)
        return 0;

    /* first byte is always 1
     * second byte usually zero, but values like 0x01 have been seen.
     * third-fourth bytes are usually zero, it's not known what they mean. A few ICON files have 0x0002 here.
     * fifth-sixth bytes are usually zero, and they correspond to the bmType field of the BITMAP structure,
     * but apparently with RT_ICON resources it can contain some other type of value like 0x001B. */
    if (data[0] == 0x01 && (data[1] & (0xFF-0x81)) == 0x00)
        return 1;

    return 0;
}

