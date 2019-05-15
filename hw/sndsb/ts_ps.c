
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

#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>

#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sndsbpnp.h>

#include "ts_pcom.h"

void sb1_sc_play_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    doubleprintf("SB 1.x single cycle DSP playback test.\n");

    timeout = T8254_REF_CLOCK_HZ * 4UL;

    for (count=0;count < 256;count++) {
        expect = 1000000UL / (unsigned long)(256 - count);

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        tlen = expect; // 1 sec
        if (tlen > sb_card->buffer_size) tlen = sb_card->buffer_size;

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        sndsb_write_dsp_timeconst(sb_card,count);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

        {
            unsigned int lv = (unsigned int)(tlen - 1UL);

            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_8BIT); /* 0x14 */
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

            doubleprintf(" - TC 0x%02X: expecting %luHz, %lub/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    }

    _cli();
    if (sb_card->irq >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
    }
    else if (sb_card->irq >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
    }
    _sti();

    sndsb_write_dsp_timeconst(sb_card,0x83); /* 8000Hz */

    sndsb_reset_dsp(sb_card);
}

void sb2_sc_play_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    if (sb_card->dsp_vmaj < 2 || (sb_card->dsp_vmaj == 2 && sb_card->dsp_vmin == 0))
        return;

    doubleprintf("SB 2.x single cycle (highspeed) DSP playback test.\n");

    timeout = T8254_REF_CLOCK_HZ * 4UL;

    for (count=0;count < 256;count++) {
        expect = 1000000UL / (unsigned long)(256 - count);

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        tlen = expect; // 1 sec
        if (tlen > sb_card->buffer_size) tlen = sb_card->buffer_size;

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        sndsb_write_dsp_timeconst(sb_card,count);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

        {
            unsigned int lv = (unsigned int)(tlen - 1UL);

            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SET_DMA_BLOCK_SIZE); /* 0x48 */
            sndsb_write_dsp(sb_card,lv);
            sndsb_write_dsp(sb_card,lv >> 8);

            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_8BIT_HISPEED); /* 0x91 */
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

            doubleprintf(" - TC 0x%02X: expecting %luHz, %lub/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    }

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
    sndsb_reset_dsp(sb_card);

    sndsb_write_dsp_timeconst(sb_card,0x83); /* 8000Hz */
}

void ess_sc_play_test(void) {
    unsigned char bytespersample = wav_16bit ? 2 : 1;
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned long count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    if (!sb_card->ess_extensions || sb_card->ess_chipset == 0)
        return;

    doubleprintf("ESS688 single cycle DSP playback test (%u bit).\n",wav_16bit?16:8);

    timeout = T8254_REF_CLOCK_HZ * 2UL;

    for (count=0;count < 256;count++) {
        if (count >= 128)
            expect = 795500UL / (256 - count);
        else
            expect = 397700UL / (128 - count);

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        tlen = expect; // 1 sec
        if (tlen > (sb_card->buffer_size / (unsigned long)bytespersample)) tlen = sb_card->buffer_size / (unsigned long)bytespersample;

        /* NTS: We ask the card to use demand ISA, 4 bytes at a time.
         *      Behavior observed on real hardware, is that if you set up
         *      DMA this way and then set up a non-auto-init DMA transfer
         *      that isn't a multiple of 4, the DMA will stop short of
         *      the full transfer and the IRQ will never fire. Therefore
         *      the transfer length must be a multiple of 4! */
        tlen -= tlen & 3;
        if (tlen == 0) tlen = 4;

        sb_card->buffer_dma_started_length = tlen * (unsigned long)bytespersample;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_ess_set_extended_mode(sb_card,1/*enable*/);
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        {
            /* ESS 688/1869 chipset specific DSP playback.
               using this mode bypasses a lot of the Sound Blaster Pro emulation
               and restrictions and allows us to run up to 48KHz 16-bit stereo */
            unsigned short t16;
            int b;

            _cli();

            /* clear IRQ */
            sndsb_interrupt_ack(sb_card,3);

            b = 0x00; /* DMA disable */
            b |= 0x00; /* no auto-init */
            b |= 0x00; /* [3]=DMA converter in ADC mode [1]=DMA read for ADC playback mode */
            sndsb_ess_write_controller(sb_card,0xB8,b);

            b = sndsb_ess_read_controller(sb_card,0xA8);
            b &= ~0xB; /* clear mono/stereo and record monitor (bits 3, 1, and 0) */
            b |= 2;	/* mono 10=mono 01=stereo */
            sndsb_ess_write_controller(sb_card,0xA8,b);

            /* NTS: The meaning of bits 1:0 in register 0xB9
             *
             *      00 single DMA transfer mode
             *      01 demand DMA transfer mode, 2 bytes/request
             *      10 demand DMA transfer mode, 4 bytes/request
             *      11 reserved
             *
             * NOTES on what happens if you set bits 1:0 (DMA transfer type) to the "reserved" 11 value:
             *
             *      ESS 688 (Sharp laptop)          Nothing, apparently. Treated the same as 4 bytes/request
             *
             *      ESS 1887 (Compaq Presario)      Triggers a hardware bug where the chip appears to fetch
             *                                      3 bytes per demand transfer but then only handle 1 byte,
             *                                      which translates to audio playing at 3x the sample rate
             *                                      it should be. NOT because the DAC is running any faster,
             *                                      but because the chip is only playing back every 3rd sample!
             *                                      This play only 3rds behavior is consistent across 8/16-bit
             *                                      PCM and mono/stereo.
             */

            b = 2;  /* demand transfer DMA 4 bytes per request */
            sndsb_ess_write_controller(sb_card,0xB9,b);

            sndsb_ess_write_controller(sb_card,0xA1,count);

            /* effectively disable the lowpass filter (NTS: 0xFF mutes the audio, apparently) */
            sndsb_ess_write_controller(sb_card,0xA2,0xFE);

            t16 = -(tlen * (uint16_t)bytespersample); /* DMA transfer count reload register value is 2's complement of length */
            sndsb_ess_write_controller(sb_card,0xA4,t16); /* DMA transfer count low */
            sndsb_ess_write_controller(sb_card,0xA5,t16>>8); /* DMA transfer count high */

            b = sndsb_ess_read_controller(sb_card,0xB1);
            b &= ~0xA0; /* clear compat game IRQ, fifo half-empty IRQs */
            b |= 0x50; /* set overflow IRQ, and "no function" */
            sndsb_ess_write_controller(sb_card,0xB1,b);

            b = sndsb_ess_read_controller(sb_card,0xB2);
            b &= ~0xA0; /* clear compat */
            b |= 0x50; /* set DRQ/DACKB inputs for DMA */
            sndsb_ess_write_controller(sb_card,0xB2,b);

            b = 0x51; /* enable FIFO+DMA, reserved, load signal */
	        b |= wav_16bit ? 0x20 : 0x00; /* signed complement mode or not */
            sndsb_ess_write_controller(sb_card,0xB7,b);

            b = 0x90; /* enable FIFO+DMA, reserved, load signal */
	        b |= wav_16bit ? 0x20 : 0x00; /* signed complement mode or not */
            b |= 0x40; /* [3]=stereo [6]=!stereo */
            b |= wav_16bit ? 0x04 : 0x00; /* [2]=16bit */
            sndsb_ess_write_controller(sb_card,0xB7,b);

            b = sndsb_ess_read_controller(sb_card,0xB8);
            sndsb_ess_write_controller(sb_card,0xB8,b | 1);
        }

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

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
            int b;

            b = sndsb_ess_read_controller(sb_card,0xB8);
            if (b != -1) {
                b &= ~0x01; /* stop DMA */
                sndsb_ess_write_controller(sb_card,0xB8,b);
            }
        }

        {
            double t = (double)time / T8254_REF_CLOCK_HZ;
            double rate = (double)bytes / t;

            doubleprintf(" - TC 0x%02lX: expecting %luHz, %lu%c/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,wav_16bit?'w':'b',t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8) / (unsigned long)bytespersample; /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    }

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

void sb16_sc_play_test(void) {
    unsigned char bytespersample = wav_16bit ? 2 : 1;
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned long count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;
    int dma;

    if (wav_16bit && sb_card->dma16 >= 4)
        dma = sb_card->dma16;
    else
        dma = sb_card->dma8;

    if (sb_card->dsp_vmaj >= 4) /* Sound Blaster 16 */
        { }
    else if (sb_card->is_gallant_sc6600) /* Reveal SC-4000 / Gallant SC-6600 */
        { }
    else 
        return;

    doubleprintf("SB16 4.x single cycle DSP playback test (%u bit).\n",wav_16bit?16:8);

    timeout = T8254_REF_CLOCK_HZ * 2UL;

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

        tlen = expect; // 1 sec
        if (tlen < 4000UL) tlen = 4000UL;
        if (tlen > (sb_card->buffer_size / (unsigned long)bytespersample)) tlen = sb_card->buffer_size / (unsigned long)bytespersample;

        sb_card->buffer_dma_started_length = tlen * (unsigned long)bytespersample;
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
            if (wav_16bit) {
                if (sb_card->is_gallant_sc6600)
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_AUTOINIT_DMA_DAC_OUT_16BIT); /* 0xB6 */
                else
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_DMA_DAC_OUT_16BIT); /* 0xB0 */

                sndsb_write_dsp(sb_card,0x10); /* mode (16-bit signed PCM) */
            }
            else {
                if (sb_card->is_gallant_sc6600)
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_AUTOINIT_DMA_DAC_OUT_8BIT); /* 0xC6 */
                else
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_DMA_DAC_OUT_8BIT); /* 0xC0 */

                sndsb_write_dsp(sb_card,0x00); /* mode (8-bit unsigned PCM) */
            }

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

            doubleprintf(" - Rate 0x%04lX: expecting %luHz, %lu%c/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,wav_16bit?'w':'b',t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        count += 0x80; /* count by 128 because enumerating all would take too long */
        if (count > 0xFFFFUL) break;

        continue;
x_timeout:
        d = d8237_read_count(dma) / (unsigned long)bytespersample; /* counts DOWNWARD */
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

    sndsb_write_dsp_outrate(sb_card,8000U);
}

void direct_dac_test(void) {
    unsigned long time,bytes;
    unsigned char FAR *ptr;
    unsigned int i,count;
    unsigned int pc,c;

    doubleprintf("Direct DAC playback test.\n");

    /* FIXME: Why is the final rate SLOWER in DOSBox-X in 386 protected mode? */

    sndsb_reset_dsp(sb_card);
    sndsb_write_dsp(sb_card,0xD1); /* speaker on */

    _cli();
    time = 0;
    bytes = 0;
    c = read_8254(T8254_TIMER_INTERRUPT_TICK);
    for (count=0;count < 3;count++) {
        ptr = sb_dma->lin;
        for (i=0;i < sb_dma->length;i++) {
            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DIRECT_DAC_OUT); /* 0x10 */
            sndsb_write_dsp(sb_card,*ptr++);

            pc = c;
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);
            time += (unsigned long)((pc - c) & 0xFFFFU); /* remember: it counts DOWN. assumes full 16-bit count */
        }
        bytes += sb_dma->length;
    }
    _sti();

    if (time == 0UL) time = 1;

    {
        double t = (double)time / T8254_REF_CLOCK_HZ;
        double rate = (double)bytes / t;

        doubleprintf(" - %lu bytes played in %.3f seconds\n",(unsigned long)bytes,t);
        doubleprintf(" - Sample rate is %.3fHz\n",rate);
    }
}

void sb1_sc_play_adpcm4_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    doubleprintf("SB 1.x ADPCM 4-bit single cycle DSP playback test.\n");

    timeout = T8254_REF_CLOCK_HZ * 4UL;

    for (count=0;count < 256;count++) {
        expect = 1000000UL / (unsigned long)(256 - count);

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        tlen = (expect / 2UL) + 1; // 1 sec
        if (tlen > sb_card->buffer_size) tlen = sb_card->buffer_size;

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        sndsb_write_dsp_timeconst(sb_card,count);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

        {
            unsigned int lv = (unsigned int)(tlen - 1UL);

            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_ADPCM_4BIT_REF); /* 0x75 */
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
            double rate = (double)(((bytes - 1UL) * 2UL) + 1UL) / t; /* 2 samples/byte + 1 reference */

            doubleprintf(" - TC 0x%02X: expecting %luHz, %lub/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    }

    _cli();
    if (sb_card->irq >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
    }
    else if (sb_card->irq >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
    }
    _sti();

    sndsb_write_dsp_timeconst(sb_card,0x83); /* 8000Hz */

    sndsb_reset_dsp(sb_card);
}

void sb1_sc_play_adpcm26_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    doubleprintf("SB 1.x ADPCM 2.6-bit single cycle DSP playback test.\n");

    timeout = T8254_REF_CLOCK_HZ * 4UL;

    for (count=0;count < 256;count++) {
        expect = 1000000UL / (unsigned long)(256 - count);

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        tlen = (expect / 3UL) + 1; // 1 sec
        if (tlen > sb_card->buffer_size) tlen = sb_card->buffer_size;

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        sndsb_write_dsp_timeconst(sb_card,count);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

        {
            unsigned int lv = (unsigned int)(tlen - 1UL);

            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_ADPCM_26BIT_REF); /* 0x77 */
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
            double rate = (double)(((bytes - 1UL) * 3UL) + 1UL) / t; /* 3 samples/byte + 1 reference */

            doubleprintf(" - TC 0x%02X: expecting %luHz, %lub/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    }

    _cli();
    if (sb_card->irq >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
    }
    else if (sb_card->irq >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
    }
    _sti();

    sndsb_write_dsp_timeconst(sb_card,0x83); /* 8000Hz */

    sndsb_reset_dsp(sb_card);
}

void sb1_sc_play_adpcm2_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int count;
    unsigned int pc,c;
    unsigned long d;
    uint32_t irqc;

    doubleprintf("SB 1.x ADPCM 2-bit single cycle DSP playback test.\n");

    timeout = T8254_REF_CLOCK_HZ * 4UL;

    for (count=0;count < 256;count++) {
        expect = 1000000UL / (unsigned long)(256 - count);

        _cli();
        if (sb_card->irq >= 8) {
            p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
        }
        else if (sb_card->irq >= 0) {
            p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
        }
        _sti();

        tlen = (expect / 4UL) + 1; // 1 sec
        if (tlen > sb_card->buffer_size) tlen = sb_card->buffer_size;

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_setup_dma(sb_card);
        irqc = sb_card->irq_counter;

        sndsb_write_dsp_timeconst(sb_card,count);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);
        bytes = tlen;
        time = 0;
        _sti();

        {
            unsigned int lv = (unsigned int)(tlen - 1UL);

            sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_ADPCM_2BIT_REF); /* 0x17 */
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
            double rate = (double)(((bytes - 1UL) * 4UL) + 1UL) / t; /* 4 samples/byte + 1 reference */

            doubleprintf(" - TC 0x%02X: expecting %luHz, %lub/%.3fs @ %.3fHz\n",count,expect,(unsigned long)bytes,t,rate);
        }

        if (kbhit()) {
            if (getch() == 27)
                break;
        }

        continue;
x_timeout:
        d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
        if (d > tlen) d = 0; /* terminal count */
        d = tlen - d;

        if (irqc == sb_card->irq_counter && d == 0) bytes = 0; /* nothing happened if no IRQ and counter never changed */
        else if (bytes > d) bytes = d;
        goto x_complete;
    }

    _cli();
    if (sb_card->irq >= 8) {
        p8259_OCW2(8,P8259_OCW2_SPECIFIC_EOI | (sb_card->irq & 7));
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | 2);
    }
    else if (sb_card->irq >= 0) {
        p8259_OCW2(0,P8259_OCW2_SPECIFIC_EOI | sb_card->irq);
    }
    _sti();

    sndsb_write_dsp_timeconst(sb_card,0x83); /* 8000Hz */

    sndsb_reset_dsp(sb_card);
}

int main(int argc,char **argv) {
    if (common_sb_init() != 0)
        return 1;

    printf("Test results will be written to TS_PS.TXT\n");

    report_fp = fopen("TS_PS.TXT","w");
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

    direct_dac_test();
    sb1_sc_play_test();
    sb2_sc_play_test();
    sb16_sc_play_test();
    ess_sc_play_test();

    wav_16bit = 1;
    sb_card->buffer_16bit = wav_16bit;
    realloc_dma_buffer();
	sndsb_assign_dma_buffer(sb_card,sb_dma);
    generate_1khz_sine16();

    ess_sc_play_test();
    sb16_sc_play_test();

    wav_16bit = 0;
    sb_card->buffer_16bit = wav_16bit;
    realloc_dma_buffer();
	sndsb_assign_dma_buffer(sb_card,sb_dma);

    generate_1khz_sine_adpcm4();
    sb1_sc_play_adpcm4_test();

    generate_1khz_sine_adpcm26();
    sb1_sc_play_adpcm26_test();

    generate_1khz_sine_adpcm2();
    sb1_sc_play_adpcm2_test();

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

