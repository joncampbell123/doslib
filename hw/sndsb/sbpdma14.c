
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>

/* this MUST follow conio.h */
#define DOSLIB_REDEFINE_INP
#include <hw/cpu/liteio.h>

/* Try to detect DMA channel, by playing silent audio blocks via DMA and
 * watching whether or not the DMA pointer moves. This method is most likely
 * to work on both Creative hardware and SB clones. */
static const unsigned char      try_dma[] = {0,1,3};

void sndsb_probe_dma8_14(struct sndsb_ctx *cx) {
    struct dma_8237_allocation *dma = NULL; /* DMA buffer */
    unsigned int len = 100,i,j;
    uint8_t ch,dmap=0,iter=0;
    uint8_t old_mask;

    if (cx->dma8 >= 0) return;
    if (!sndsb_reset_dsp(cx)) return;

    dma = dma_8237_alloc_buffer(len);
    if (dma == NULL) return;

#if TARGET_MSDOS == 32
    memset(dma->lin,128,len);
#else
    _fmemset(dma->lin,128,len);
#endif

    _cli();
    old_mask = inp(d8237_ioport(/*channel*/1,/*port*/0xF));
    if (old_mask == 0xff) old_mask = 0;

    while (dmap < sizeof(try_dma)) {
        ch = try_dma[dmap];

        /* any DMA? and did the byte returned match the patttern? */
        for (iter=0;iter < 10;iter++) {
            _cli();
            outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

            outp(d8237_ioport(ch,D8237_REG_W_WRITE_MODE),
                    D8237_MODER_CHANNEL(ch) | D8237_MODER_TRANSFER(D8237_MODER_XFER_READ) | D8237_MODER_MODESEL(D8237_MODER_MODESEL_SINGLE));

            inp(d8237_ioport(ch,D8237_REG_R_STATUS));
            d8237_write_count(ch,len);
            d8237_write_base(ch,dma->phys);
            outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch)); /* unmask */
            _sti();

            /* use DSP command 0x14 */
            sndsb_interrupt_ack(cx,sndsb_interrupt_reason(cx));
            sndsb_write_dsp_timeconst(cx,0xD3); /* 22050Hz */
            sndsb_write_dsp(cx,0x14);
            sndsb_write_dsp(cx,(len - 1));
            sndsb_write_dsp(cx,(len - 1) >> 8);

            /* wait 100ms */
            j=10;
            do {
                t8254_wait(t8254_us2ticks(10000));
                i = d8237_read_count(ch)&0xFFFFUL;
                if (i == 0U || i == 0xFFFFU) break;
                if (--j == 0) break;
            } while (1);

            outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

            if (i == 0U || i == 0xFFFFU) {
                /* it worked */
            }
            else {
                sndsb_reset_dsp(cx);
                break;
            }
        }

        if (iter == 10) {
            cx->dma8 = ch;
            if (cx->ess_chipset != SNDSB_ESS_NONE || cx->is_gallant_sc6600) cx->dma16 = cx->dma8;
            break;
        }

        dmap++;
    }

    outp(d8237_ioport(/*channel*/1,0xF),old_mask);
    _sti();

    dma_8237_free_buffer(dma);
}

