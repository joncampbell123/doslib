
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>

/* this MUST follow conio.h */
#define DOSLIB_REDEFINE_INP
#include <hw/cpu/liteio.h>

// bizarre pattern table, copied from DOSBox source code.
// converted from signed int to unsigned char for space.
//
// original table:
//
// static int const E2_incr_table[4][9] = {
//   {  0x01, -0x02, -0x04,  0x08, -0x10,  0x20,  0x40, -0x80, -106 },
//   { -0x01,  0x02, -0x04,  0x08,  0x10, -0x20,  0x40, -0x80,  165 },
//   { -0x01,  0x02,  0x04, -0x08,  0x10, -0x20, -0x40,  0x80, -151 },
//   {  0x01, -0x02,  0x04, -0x08, -0x10,  0x20, -0x40,  0x80,   90 }
// };
static const unsigned char E2_incr_table[4][9] = {
    { 0x01, 0xFE, 0xFC, 0x08, 0xF0, 0x20, 0x40, 0x80, 0x96 },
    { 0xFF, 0x02, 0xFC, 0x08, 0x10, 0xE0, 0x40, 0x80, 0xA5 },
    { 0xFF, 0x02, 0x04, 0xF8, 0x10, 0xE0, 0xC0, 0x80, 0x69 },
    { 0x01, 0xFE, 0x04, 0xF8, 0xF0, 0x20, 0xC0, 0x80, 0x5A }
};

/* Try to detect DMA channel, using undocumented Creative Sound Blaster "DMA test" command 0xE2.
 * The byte we give with it comes back added to an internal register, and we have to validate that
 * it's the Sound Blaster by matching it on our end when it comes back via DMA. This is weird
 * and convoluted enough that I don't expect SB clones to implement it properly if at all, but it
 * is an option. */
static const unsigned char      try_dma[] = {0,1,3};

void sndsb_probe_dma8_E2(struct sndsb_ctx *cx) {
    struct dma_8237_allocation *dma = NULL; /* DMA buffer */
    uint8_t ch,dmap=0,iter=0,iterr;
    uint8_t reg,regi=0,i;
    uint8_t test_byte;
    uint8_t old_mask;

    if (cx->dma8 >= 0) return;
    if (cx->sbos || cx->mega_em) return; // NTS: doesn't work with SBOS or MEGA-EM, there's no point in running the test
    if (!sndsb_reset_dsp(cx)) return;
    reg = 0xAA;

    dma = dma_8237_alloc_buffer(64);
    if (dma == NULL) return;

    _cli();
    old_mask = inp(d8237_ioport(/*channel*/1,/*port*/0xF));
    if (old_mask == 0xff) old_mask = 0;

    while (dmap < sizeof(try_dma)) {
        ch = try_dma[dmap];

        /* any DMA? and did the byte returned match the patttern? */
        iterr = 0;
        for (iter=0;iter < 10;iter++) {
            _cli();
            outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

            test_byte = (uint8_t)read_8254_ncli(0);

            /* predict the value being returned */
            for (i=0;i < 8;i++) {
                if ((test_byte >> i) & 1) reg += E2_incr_table[regi][i];
            }
            reg += E2_incr_table[regi][8];
            if ((++regi) == 4) regi = 0;

            dma->lin[0] = ~reg;

            outp(d8237_ioport(ch,D8237_REG_W_WRITE_MODE),
                    D8237_MODER_CHANNEL(ch) | D8237_MODER_TRANSFER(D8237_MODER_XFER_WRITE) | D8237_MODER_MODESEL(D8237_MODER_MODESEL_SINGLE));

            inp(d8237_ioport(ch,D8237_REG_R_STATUS));
            d8237_write_count(ch,1);
            d8237_write_base(ch,dma->phys);
            outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch)); /* unmask */
            _sti();

            /* use DSP command 0xE2 */
            sndsb_interrupt_ack(cx,sndsb_interrupt_reason(cx));
            sndsb_write_dsp(cx,0xE2);
            sndsb_write_dsp(cx,test_byte);

            /* wait 5ms */
            t8254_wait(t8254_us2ticks(5000));

            outp(d8237_ioport(ch,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(ch) | D8237_MASK_SET); /* mask */

#if 0//DEBUG
            fprintf(stderr,"count=%lu lin=%02x exp=%02x changed=%s ch=%u it=%u\n",
                (unsigned long)d8237_read_count(ch),dma->lin[0],reg,dma->lin[0]==((~reg)&0xFFu)?"no":"yes",ch,iter);
#endif

            if (d8237_read_count(ch) != 1 && dma->lin[0] == reg) {
                /* match */
            }
            else {
                /* lost track */
                sndsb_reset_dsp(cx);
                reg = 0xAA;
                regi = 0;
                iter = 0;
                if ((++iterr) >= 3) break;
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

