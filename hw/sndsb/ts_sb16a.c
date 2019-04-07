/* Test all SB16 sample rates (and SB/SBPro constants).
 * This time, we read it back from the DSP chip's internal RAM using undocumented DSP commands */

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
#include <math.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>

#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

#include "ts_pcom.h"

unsigned char fast_mode = 0;

/* TODO: Move into SNDSB library */
int sndsb_sb16_8051_mem_read(struct sndsb_ctx* cx,const unsigned char idx) {
	if (sb_card->dsp_vmaj < 4) return -1;

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

/* TODO: Move into SNDSB library */
int sndsb_sb16_8051_mem_write(struct sndsb_ctx* cx,const unsigned char idx,const unsigned char c) {
	if (sb_card->dsp_vmaj < 4) return -1;

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

uint16_t sndsb_sb16_read_dsp_internal_rate(struct sndsb_ctx* cx) {
    uint16_t r;
    int i;

    i = sndsb_sb16_8051_mem_read(cx,0x13); /* Rate low (see https://github.com/joncampbell123/dosbox-x/issues/1044) */
    if (i < 0) return ~0U;
    r = (uint16_t)(i & 0xFFu);

    i = sndsb_sb16_8051_mem_read(cx,0x14); /* Rate high (see https://github.com/joncampbell123/dosbox-x/issues/1044) */
    if (i < 0) return ~0U;
    r |= ((uint16_t)(i & 0xFFu)) << 8u;

    return r;
}

void sb16_rate_test(void) {
    uint16_t setv,getv;

    sndsb_reset_dsp(sb_card);

    doubleprintf("SB16 4.x sample rate constant test.\n");

    getv = sndsb_sb16_read_dsp_internal_rate(sb_card);

    doubleprintf("Initial sample rate is %u\n",getv);

    setv = 0;
    do {
        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        sndsb_write_dsp_outrate(sb_card,setv);
        getv = sndsb_sb16_read_dsp_internal_rate(sb_card);

        fprintf(report_fp," - Rate %u: Result %u\n",setv,getv);
        if ((setv & 0xFF) == 0) 
            printf(" - Rate %u: Result %u\n",setv,getv);

        setv++;
        if (setv == 0u) break;
    } while(1);
}

int main(int argc,char **argv) {
    if (common_sb_init() != 0)
        return 1;

    report_fp = fopen("TS_SB16A.TXT","w");
    if (report_fp == NULL) return 1;

    sb16_rate_test();

    printf("Test complete.\n");
    fclose(report_fp);

    free_dma_buffer();

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

