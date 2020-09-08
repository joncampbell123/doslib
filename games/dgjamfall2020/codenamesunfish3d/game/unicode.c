
#include <stdio.h>
#include <stdint.h>

/* decode UTF-8 unicode character.
 * returns 0 at end of string or on invalid encoding */
/* [https://en.wikipedia.org/wiki/UTF-8] */
uint32_t utf8decode(const char ** const ptr) {
    const char *p = *ptr;
    uint32_t ret = 0;

    if (p == NULL) return 0;
    ret = (unsigned char)(*p++);

    /* 0xFE-0xFF            invalid
     * 0xFC-0xFD            6-byte UTF-8 TODO
     * 0xF8-0xFB            5-byte UTF-8 TODO
     * 0xF0-0xF7            4-byte UTF-8 TODO
     * 0xE0-0xEF            3-byte UTF-8
     * 0xC0-0xDF            2-byte UTF-8
     * 0x80-0xBF            invalid (we're in the middle of a UTF-8 char)
     * 0x00-0x7F            1-byte plain ASCII */
    if (ret >= 0xF0) {
        ret = 0;
    }
    else if (ret >= 0xE0) { /* 3-byte */
        /* [1110 xxxx] [10xx xxxx] [10xx xxxx]  4+6+6 = 16 bits */
        ret = (ret & 0xFul) << 12ul;
        if (((*p) & 0xC0) != 0x80) { ret=0; goto stop; }
        ret += (((unsigned char)(*p++)) & 0x3Ful) << 6ul;
        if (((*p) & 0xC0) != 0x80) { ret=0; goto stop; }
        ret += ((unsigned char)(*p++)) & 0x3Ful;
    }
    else if (ret >= 0xC0) { /* 2-byte */
        /* [110x xxxx] [10xx xxxx]  5+6 = 11 bits */
        ret = (ret & 0x1Ful) << 6ul;
        if (((*p) & 0xC0) != 0x80) { ret=0; goto stop; }
        ret += ((unsigned char)(*p++)) & 0x3Ful;
    }
    else if (ret >= 0x80) { /* invalid */
        ret = 0;
    }

stop:
    *ptr = p;
    return ret;
}

