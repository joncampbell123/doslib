
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <direct.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/sndsb/sndsb.h>

int sndsb_sb16_8051_mem_read(struct sndsb_ctx* cx,const unsigned char idx) {
	if (cx->dsp_vmaj < 4) return -1;

    if (sndsb_write_dsp(cx,0xF9) < 0) {
        sndsb_reset_dsp(cx);
        return -1;
    }
    if (sndsb_write_dsp(cx,idx) < 0) {
        sndsb_reset_dsp(cx);
        return -1;
    }

    return sndsb_read_dsp(cx);
}

int sndsb_sb16_8051_mem_write(struct sndsb_ctx* cx,const unsigned char idx,const unsigned char c) {
	if (cx->dsp_vmaj < 4) return -1;

    if (sndsb_write_dsp(cx,0xFA) < 0) {
        sndsb_reset_dsp(cx);
        return -1;
    }
    if (sndsb_write_dsp(cx,idx) < 0) {
        sndsb_reset_dsp(cx);
        return -1;
    }
    if (sndsb_write_dsp(cx,c) < 0) {
        sndsb_reset_dsp(cx);
        return -1;
    }

    /* DSP does not return data in response */
    return 0;
}

