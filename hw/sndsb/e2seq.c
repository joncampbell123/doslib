
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
#include <hw/sndsb/sb16asp.h>
#include <hw/sndsb/sndsbpnp.h>

#include "ts_pcom.h"

int dma_nocycle = 0;
int dma_toomuch = 0;

int do_e2(unsigned char b) {
    const unsigned char ch = (unsigned char)sb_card->dma8;
    unsigned int cn;
    int i;

    if (sb_card->dma8 < 0)
        return -1;

    sndsb_interrupt_ack(sb_card,sndsb_interrupt_reason(sb_card));

    {
        _cli();
        outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

        outp(d8237_ioport(ch,D8237_REG_W_WRITE_MODE),
                D8237_MODER_CHANNEL(ch) | D8237_MODER_TRANSFER(D8237_MODER_XFER_WRITE) | D8237_MODER_MODESEL(D8237_MODER_MODESEL_SINGLE));

        inp(d8237_ioport(ch,D8237_REG_R_STATUS));
        d8237_write_count(ch,sb_dma->length);
        d8237_write_base(ch,sb_dma->phys);
        outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch)); /* unmask */
        _sti();
    }

    if (sndsb_write_dsp(sb_card,0xE2) < 0)
        goto fail;
    if (sndsb_write_dsp(sb_card,b) < 0)
        goto fail;

    /* wait 5ms */
    t8254_wait(t8254_us2ticks(5000));

    /* what came back? */
    cn = d8237_read_count(ch);
    if (cn > sb_dma->length) cn = sb_dma->length;
    cn = sb_dma->length - cn;
    if (cn == 0) {
        /* nothing happened, no terminal count */
        dma_nocycle++;
        goto fail;
    }
    if (cn > 1) {
        /* After deliberate DMA failure by setting wrong DMA channel test, DOSBox-X appears to DMA cycle one extra byte,
         * which causes wrong results. */
        dma_toomuch++;
    }

    i = (int)sb_dma->lin[0];

    outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */
    return i;

fail:
    outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */
    sndsb_reset_dsp(sb_card);
    return -1;
}

int main(int argc,char **argv) {
    unsigned char e2byte;
    int res,i;

    if (common_sb_init() != 0)
        return 1;

    assert(sb_card != NULL);

    printf("What byte do I send with command 0xE2? "); fflush(stdout);
    {
        char line[64];
        if (fgets(line,sizeof(line),stdin) == NULL) return 1;
        e2byte = (unsigned char)strtoul(line,NULL,0);
    }
    printf("Sending 0x%02x\n",e2byte);

    buffer_limit = 256; /* do not need too much buffer for this test */
    realloc_dma_buffer();
	sndsb_assign_dma_buffer(sb_card,sb_dma);

    printf("Result:\n");
	sndsb_reset_dsp(sb_card);
    for (i=0;i < 256;i++) {
        if ((i&15) == 0) printf("    %02x: ",i);

        res = do_e2(e2byte);
        if (res >= 0) printf("%02x ",(unsigned char)res);
        else          printf("XX ");

        if ((i&15) == 15) printf("\n");

        if (kbhit()) {
            int c = getch();
            if (c == 27) {
                printf("\n");
                break;
            }
        }

        fflush(stdout);
    }

    if (dma_nocycle > 0)
        printf("DMA failed to cycle %u times\n",dma_nocycle);
    if (dma_toomuch > 0)
        printf("DMA returned too many bytes %u times. Run test again for correct results.\n",dma_toomuch);

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

