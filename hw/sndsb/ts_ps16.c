/* extended sample rate test for SB16 to test ALL sample rates (0-65535).
 * this test might take awhile even despite shortcuts. */

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

void sb16_sc_play_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned long count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    if (sb_card->dsp_vmaj >= 4) /* Sound Blaster 16 */
        { }
    else if (sb_card->is_gallant_sc6600) /* Reveal SC-4000 / Gallant SC-6600 */
        { }
    else 
        return;

    doubleprintf("SB16 4.x single cycle DSP playback test (8 bit) in %s mode.\n",fast_mode?"fast":"accurate");
    printf("This test will take a LONG time!\n");
    printf("Hit ESC at any time to break out.\n");

    count = 0;

    do {
        expect = count;

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        if (fast_mode) {
            timeout = T8254_REF_CLOCK_HZ / 10UL + (T8254_REF_CLOCK_HZ / 100UL); /* 110ms */
            tlen = expect / 10UL; // 100ms

            // SB16 is pretty consistent about capping the lower rate at 4800Hz
            if (tlen < (4800UL / 10UL)) tlen = 4800UL / 10UL;
        }
        else {
            /* we need longer test periods for more precision */
            timeout = T8254_REF_CLOCK_HZ + (T8254_REF_CLOCK_HZ / 2UL); /* 1500ms */
            tlen = expect; // 1 sec

            // SB16 is pretty consistent about capping the lower rate at 4800Hz
            if (tlen < 4800UL) tlen = 4800UL;
        }

        if (tlen > sb_card->buffer_size) tlen = sb_card->buffer_size;

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        sndsb_write_dsp_outrate(sb_card,count);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

        {
            unsigned int lv = (unsigned int)(tlen - 1UL);

            /* NTS: Reveal SC-4000 (Gallant 6600) cards DO support SB16 but only specific commands.
             *      Command 0xC0 is not recognized, but command 0xC6 works. */
            if (sb_card->is_gallant_sc6600)
                sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_AUTOINIT_DMA_DAC_OUT_8BIT); /* 0xC6 */
            else
                sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_DMA_DAC_OUT_8BIT); /* 0xC0 */

            sndsb_write_dsp(sb_card,0x00); /* mode (8-bit unsigned PCM) */
            sndsb_write_dsp(sb_card,lv);
            sndsb_write_dsp(sb_card,lv >> 8);
        }

        while (1) {
            if (irqc != sb_card->irq_counter)
                break;

            _cli();
            pc = c;
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);
            time += (unsigned long)((pc - c) & 0xFFFFU); /* remember: it counts DOWN. assumes full 16-bit count */
            _sti();

            if (time >= timeout) goto x_timeout;
        }

x_complete:
        if (time == 0UL) time = 1;

        {
            double t = (double)time / T8254_REF_CLOCK_HZ;
            double rate = (double)bytes / t;

            doubleprintf(" - Rate 0x%04lX: expecting %luHz, %lub/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        if (count < 0xFFFFUL)
            count++;
        else
            break;

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    } while (1);

    _cli();
    if (sb_card->irq >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
    }
    else if (sb_card->irq >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
    }
    _sti();

    sndsb_reset_dsp(sb_card);
}

int main(int argc,char **argv) {
    printf("Two test modes are available.\n");
    printf("Select (F)ast or (A)ccurate.\n");

    {
        int i;

        i = tolower(getch());
        if (i == 'f')
            fast_mode = 1;
        else if (i == 'a')
            fast_mode = 0;
        else
            return 1;
    }

    if (common_sb_init() != 0)
        return 1;

    report_fp = fopen("TS_PS16.TXT","w");
    if (report_fp == NULL) return 1;

    if (sb_card->irq != -1) {
        old_irq_masked = p8259_is_masked(sb_card->irq);
        if (vector_is_iret(irq2int(sb_card->irq)))
            old_irq_masked = 1;

        old_irq = _dos_getvect(irq2int(sb_card->irq));
        _dos_setvect(irq2int(sb_card->irq),sb_irq);
        p8259_unmask(sb_card->irq);
    }

    wav_16bit = 0;
    sb_card->buffer_16bit = wav_16bit;
    realloc_dma_buffer();
	sndsb_assign_dma_buffer(sb_card,sb_dma);
    generate_1khz_sine();

    sb16_sc_play_test();

	if (sb_card->irq >= 0 && old_irq_masked)
		p8259_mask(sb_card->irq);

	if (sb_card->irq != -1)
		_dos_setvect(irq2int(sb_card->irq),old_irq);

    printf("Test complete.\n");
    fclose(report_fp);

    free_dma_buffer();

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

