

#include <hw/sndsb/sndsb.h>

unsigned int sndsb_bread_dsp(struct sndsb_ctx * const cx,unsigned char *buf,unsigned int count) {
	register unsigned int readcount = 0;
    register int c;

    while ((count--) != 0U) {
        if ((c=sndsb_read_dsp(cx)) >= 0) {
            *buf++ = (unsigned char)c;
            readcount++;
        }
        else {
            break;
        }
    }

	return readcount;
}

unsigned int sndsb_bwrite_dsp(struct sndsb_ctx * const cx,const unsigned char *buf,unsigned int count) {
    register unsigned int writecount = 0;

    while ((count--) > 0) {
        if (sndsb_write_dsp(cx,*buf++)) {
            writecount++;
        }
        else {
            break;
        }
    }

	return writecount;
}

