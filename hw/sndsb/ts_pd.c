
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

static FILE*                            report_fp = NULL;

static struct dma_8237_allocation*      sb_dma = NULL; /* DMA buffer */

static struct sndsb_ctx*                sb_card = NULL;

static unsigned char far                devnode_raw[4096];

static uint32_t                         buffer_limit = 0xF000UL;

static unsigned int                     wav_sample_rate = 4000;
static unsigned char                    wav_stereo = 0;
static unsigned char                    wav_16bit = 0;

static void free_dma_buffer() {
    if (sb_dma != NULL) {
        dma_8237_free_buffer(sb_dma);
        sb_dma = NULL;
    }
}

static unsigned char old_irq_masked = 0;
static void (interrupt *old_irq)() = NULL;
static void interrupt sb_irq() {
	unsigned char c;

	sb_card->irq_counter++;

	/* ack soundblaster DSP if DSP was the cause of the interrupt */
	/* NTS: Experience says if you ack the wrong event on DSP 4.xx it
	   will just re-fire the IRQ until you ack it correctly...
	   or until your program crashes from stack overflow, whichever
	   comes first */
	c = sndsb_interrupt_reason(sb_card);
	sndsb_interrupt_ack(sb_card,c);

	/* FIXME: The sndsb library should NOT do anything in
	   send_buffer_again() if it knows playback has not started! */
	/* for non-auto-init modes, start another buffer */
	sndsb_irq_continue(sb_card,c);

	/* NTS: we assume that if the IRQ was masked when we took it, that we must not
	 *      chain to the previous IRQ handler. This is very important considering
	 *      that on most DOS systems an IRQ is masked for a very good reason---the
	 *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
	 *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
	 *      not advised! */
	if (old_irq_masked || old_irq == NULL) {
		/* ack the interrupt ourself, do not chain */
		if (sb_card->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		old_irq();
	}
}

static void realloc_dma_buffer() {
    uint32_t choice;
    int8_t ch;

    free_dma_buffer();

    ch = sndsb_dsp_playback_will_use_dma_channel(sb_card,wav_sample_rate,wav_stereo,wav_16bit);

    if (ch >= 4)
        choice = sndsb_recommended_16bit_dma_buffer_size(sb_card,buffer_limit);
    else
        choice = sndsb_recommended_dma_buffer_size(sb_card,buffer_limit);

    do {
        if (ch >= 4)
            sb_dma = dma_8237_alloc_buffer_dw(choice,16);
        else
            sb_dma = dma_8237_alloc_buffer_dw(choice,8);

        if (sb_dma == NULL) choice -= 4096UL;
    } while (sb_dma == NULL && choice > 4096UL);

    if (!sndsb_assign_dma_buffer(sb_card,sb_dma))
        return;
    if (sb_dma == NULL)
        return;
}

void generate_1khz_sine(void) {
    unsigned int i,l;

    printf("Generating tone...\n");

    l = (unsigned int)sb_dma->length;
    for (i=0;i < l;i++)
        sb_dma->lin[i] =
            (unsigned char)((sin(((double)i * 3.14159 * 2) / 100) * 64) + 128);
}

void generate_1khz_sine16(void) {
    unsigned int i,l;

    printf("Generating tone...\n");

    l = (unsigned int)sb_dma->length / 2U;
    for (i=0;i < l;i++)
        ((uint16_t FAR*)sb_dma->lin)[i] =
            (uint16_t)((int16_t)(sin(((double)i * 3.14159 * 2) / 100) * 16384));
}

static char ptmp[256];

void doubleprintf(const char *fmt,...) {
    va_list va;

    va_start(va,fmt);
    vsnprintf(ptmp,sizeof(ptmp),fmt,va);
    va_end(va);

    fputs(ptmp,stdout);
    fputs(ptmp,report_fp);
}

#if TARGET_MSDOS == 32
# define MAX_RECORD (128*1024)
#else
# define MAX_RECORD 8192
#endif

struct dma_xfer_rec_t {
    uint16_t        dma_pos;
    uint32_t        timer_pos;
};

static struct dma_xfer_rec_t record[MAX_RECORD],*record_pos,*record_read,*record_max;

static unsigned char sb1_tc_rates[] = {
    0x06,           // 4000Hz
    0x83,           // 8000Hz
    0xCE            // 20000Hz
};

/* if this function returns 1, the record log is full.
 * it will warn you only once! */
static inline int record_entry(const uint16_t dma_pos,const unsigned long time) {
    record_pos->dma_pos = dma_pos;
    record_pos->timer_pos = time;

    return (++record_pos == record_max);
}

void sb1_sc_play_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int pc,c,iter;
    unsigned int count,lv;
    unsigned long pd,d;

    doubleprintf("SB 1.x DMA single cycle DSP test.\n");

    timeout = T8254_REF_CLOCK_HZ * 2UL;
    record_max = &record[MAX_RECORD];

    for (count=0;count < (sizeof(sb1_tc_rates)/sizeof(sb1_tc_rates[0]));count++) {
        expect = 1000000UL / (unsigned long)(256 - sb1_tc_rates[count]);
        record_pos = record;

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

        printf("Starting test... tlen=%lu dmalen=%lu\n",(unsigned long)tlen,(unsigned long)sb_card->buffer_size);

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_write_dsp(sb_card,0x10); /* direct DAC reset to neutral output (0V) */
        sndsb_write_dsp(sb_card,0x80);
        sndsb_setup_dma(sb_card);

        sndsb_write_dsp_timeconst(sb_card,sb1_tc_rates[count]);

        time = 0;
        pd = d = (~0UL);

        lv = (unsigned int)(tlen - 1UL);

        sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_8BIT); /* 0x14 */
        sndsb_write_dsp(sb_card,lv);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);

        sndsb_write_dsp(sb_card,lv >> 8); /* playback begins at this write */

        while (1) {
            /* up to 16 iterations.
             * do not poll for IRQ changes, we have interrupts disabled here */
            iter = 16;
            do {
                pd = d;
                d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
            } while (--iter != 0U && pd == d);

            if (d > tlen) bytes = tlen; /* terminal count */
            else bytes = tlen - d;

            pc = c;
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);
            time += (unsigned long)((pc - c) & 0xFFFFU); /* remember: it counts DOWN. assumes full 16-bit count */

            if (pd != d) {
                if (record_entry((uint16_t)bytes,time)) break; /* break if log is full. it will warn only once */
            }
            else if (time >= timeout) {
                record_entry((uint16_t)bytes,time); /* may return 1 if this makes the log full. we break anyway */
                break;
            }
        }
        _sti();

        sndsb_reset_dsp(sb_card);

        doubleprintf(" - Test at %luHz, %lu bytes\n",expect,bytes);
        printf("Writing results... please wait\n"); /* The IDE-CF adapter setup in my old Pentium 133MHz is fairly slow at writing a 1MB text file */

        for (record_read=record;record_read!=record_pos;record_read++)
            fprintf(report_fp," >> POS %u, time %.6f\n",record_read->dma_pos,(double)record_read->timer_pos / T8254_REF_CLOCK_HZ);

        fprintf(report_fp,"\n");
        fflush(report_fp);

        if (kbhit()) {
            if (getch() == 27)
                break;
        }
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

static unsigned char sb2_tc_rates[] = {
    0x06,           // 4000Hz
    0x83,           // 8000Hz
    0xCE,           // 20000Hz
    0xE7            // 40000Hz
};

void sb2_sc_play_test(void) {
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int pc,c,iter;
    unsigned int count,lv;
    unsigned long pd,d;

    if (sb_card->dsp_vmaj < 2 || (sb_card->dsp_vmaj == 2 && sb_card->dsp_vmin == 0))
        return;

    doubleprintf("SB 2.x DMA single cycle DSP test.\n");

    timeout = T8254_REF_CLOCK_HZ * 2UL;
    record_max = &record[MAX_RECORD];

    for (count=0;count < (sizeof(sb2_tc_rates)/sizeof(sb2_tc_rates[0]));count++) {
        expect = 1000000UL / (unsigned long)(256 - sb2_tc_rates[count]);
        record_pos = record;

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

        printf("Starting test... tlen=%lu dmalen=%lu\n",(unsigned long)tlen,(unsigned long)sb_card->buffer_size);

        sb_card->buffer_dma_started_length = tlen;
        sb_card->buffer_dma_started = 0;

        sndsb_reset_dsp(sb_card);
        sndsb_write_dsp(sb_card,0xD1); /* speaker on */
        sndsb_write_dsp(sb_card,0x10); /* direct DAC reset to neutral output (0V) */
        sndsb_write_dsp(sb_card,0x80);
        sndsb_setup_dma(sb_card);

        sndsb_write_dsp_timeconst(sb_card,sb2_tc_rates[count]);

        time = 0;
        pd = d = (~0UL);

        lv = (unsigned int)(tlen - 1UL);

        sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SET_DMA_BLOCK_SIZE); /* 0x48 */
        sndsb_write_dsp(sb_card,lv);
        sndsb_write_dsp(sb_card,lv >> 8);

        _cli();
        c = read_8254(T8254_TIMER_INTERRUPT_TICK);

        sndsb_write_dsp(sb_card,SNDSB_DSPCMD_DMA_DAC_OUT_8BIT_HISPEED); /* 0x91 */

        while (1) {
            /* up to 16 iterations.
             * do not poll for IRQ changes, we have interrupts disabled here */
            iter = 16;
            do {
                pd = d;
                d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
            } while (--iter != 0U && pd == d);

            if (d > tlen) bytes = tlen; /* terminal count */
            else bytes = tlen - d;

            pc = c;
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);
            time += (unsigned long)((pc - c) & 0xFFFFU); /* remember: it counts DOWN. assumes full 16-bit count */

            if (pd != d) {
                if (record_entry((uint16_t)bytes,time)) break; /* break if log is full. it will warn only once */
            }
            else if (time >= timeout) {
                record_entry((uint16_t)bytes,time); /* may return 1 if this makes the log full. we break anyway */
                break;
            }
        }
        _sti();

        sndsb_reset_dsp(sb_card);

        doubleprintf(" - Test at %luHz, %lu bytes\n",expect,bytes);
        printf("Writing results... please wait\n"); /* The IDE-CF adapter setup in my old Pentium 133MHz is fairly slow at writing a 1MB text file */

        for (record_read=record;record_read!=record_pos;record_read++)
            fprintf(report_fp," >> POS %u, time %.6f\n",record_read->dma_pos,(double)record_read->timer_pos / T8254_REF_CLOCK_HZ);

        fprintf(report_fp,"\n");
        fflush(report_fp);

        if (kbhit()) {
            if (getch() == 27)
                break;
        }
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

static unsigned short sb16_rates[] = {
    4000U,          // 4000Hz
    8000U,          // 8000Hz
    16000U,         // 16000Hz
    20000U,         // 20000Hz
    40000U,         // 40000Hz
    45454U          // 45454Hz
};

void sb16_sc_play_test(void) {
    unsigned char bytespersample_shift = wav_16bit ? 1 : 0;
    unsigned char bytespersample = wav_16bit ? 2 : 1;
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned int pc,c,iter;
    unsigned int count,lv;
    unsigned long pd,d;
    unsigned char fifo;
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

    doubleprintf("SB 16 DMA single cycle DSP test %u-bit.\n",wav_16bit?16:8);

    timeout = T8254_REF_CLOCK_HZ * 2UL;
    record_max = &record[MAX_RECORD];

    for (count=0;count < (sizeof(sb16_rates)/sizeof(sb16_rates[0]));count++) {
        for (fifo=0;fifo < 2;fifo++) {
            if (sb_card->is_gallant_sc6600 && !fifo) /* SC400 cards only support the FIFO version */
                continue;

            expect = sb16_rates[count];
            record_pos = record;

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

            printf("Starting test... tlen=%lu dmalen=%lu fifo=%u\n",(unsigned long)tlen * (unsigned long)bytespersample,(unsigned long)sb_card->buffer_size,fifo);

            sb_card->buffer_dma_started_length = tlen * (unsigned long)bytespersample;
            sb_card->buffer_dma_started = 0;

            sndsb_reset_dsp(sb_card);
            sndsb_write_dsp(sb_card,0xD1); /* speaker on */
            sndsb_write_dsp(sb_card,0x10); /* direct DAC reset to neutral output (0V) */
            sndsb_write_dsp(sb_card,0x80);
            sndsb_setup_dma(sb_card);

            sndsb_write_dsp_outrate(sb_card,sb16_rates[count]);

            time = 0;
            pd = d = (~0UL);

            lv = (unsigned int)(tlen - 1UL);

            /* NTS: Reveal SC-4000 (Gallant 6600) cards DO support SB16 but only specific commands.
             *      Command 0xC0 is not recognized, but command 0xC6 works. */
            if (wav_16bit) {
                if (sb_card->is_gallant_sc6600)
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_AUTOINIT_DMA_DAC_OUT_16BIT); /* 0xB6 */
                else
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_DMA_DAC_OUT_16BIT + (fifo ? 2 : 0)); /* 0xB0 */

                sndsb_write_dsp(sb_card,0x10); /* mode (16-bit signed PCM) */
            }
            else {
                if (sb_card->is_gallant_sc6600)
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_AUTOINIT_DMA_DAC_OUT_8BIT); /* 0xC6 */
                else
                    sndsb_write_dsp(sb_card,SNDSB_DSPCMD_SB16_DMA_DAC_OUT_8BIT + (fifo ? 2 : 0)); /* 0xC0 */

                sndsb_write_dsp(sb_card,0x00); /* mode (8-bit unsigned PCM) */
            }

            sndsb_write_dsp(sb_card,0x00); /* mode (8-bit unsigned PCM) */
            sndsb_write_dsp(sb_card,lv);

            _cli();
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);

            sndsb_write_dsp(sb_card,lv >> 8);

            while (1) {
                /* up to 16 iterations.
                 * do not poll for IRQ changes, we have interrupts disabled here */
                iter = 16;
                do {
                    pd = d;
                    d = d8237_read_count(dma); /* counts DOWNWARD */
                } while (--iter != 0U && pd == d);
                d >>= (uint32_t)bytespersample_shift;

                if (d > tlen) bytes = tlen; /* terminal count */
                else bytes = tlen - d;

                pc = c;
                c = read_8254(T8254_TIMER_INTERRUPT_TICK);
                time += (unsigned long)((pc - c) & 0xFFFFU); /* remember: it counts DOWN. assumes full 16-bit count */

                if (pd != d) {
                    if (record_entry((uint16_t)bytes,time)) break; /* break if log is full. it will warn only once */
                }
                else if (time >= timeout) {
                    record_entry((uint16_t)bytes,time); /* may return 1 if this makes the log full. we break anyway */
                    break;
                }
            }
            _sti();

            sndsb_reset_dsp(sb_card);

            doubleprintf(" - Test at %luHz, %lu bytes, FIFO %s\n",expect,bytes * (unsigned long)bytespersample,fifo ? "on" : "off");
            printf("Writing results... please wait\n"); /* The IDE-CF adapter setup in my old Pentium 133MHz is fairly slow at writing a 1MB text file */

            for (record_read=record;record_read!=record_pos;record_read++)
                fprintf(report_fp," >> POS %u, time %.6f\n",record_read->dma_pos * (unsigned int)bytespersample,(double)record_read->timer_pos / T8254_REF_CLOCK_HZ);

            fprintf(report_fp,"\n");
            fflush(report_fp);

            if (kbhit()) {
                if (getch() == 27) {
                    count = 99;
                    fifo = 4;
                    break;
                }
            }
        }
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

static unsigned char ess_tc_rates[] = {
    0x1D,           // 4000Hz
    0x9D,           // 8000Hz
    0xD9,           // 20000Hz
    0xED            // 40000Hz
};

static const char *dma_xfer_str[] = {
    "single DMA",
    "demand DMA 2-byte",
    "demand DMA 4-byte",
    "reserved DMA transfer (3)"
};

void ess_sc_play_test(void) {
    unsigned char bytespersample = wav_16bit ? 2 : 1;
    unsigned long time,bytes,expect,tlen,timeout;
    unsigned char dma_xfer;
    unsigned int pc,c,iter;
    unsigned int count;
    unsigned long pd,d;
    int b;

    if (!sb_card->ess_extensions || sb_card->ess_chipset == 0)
        return;

    doubleprintf("ESS688 DMA single cycle DSP test %u-bit.\n",wav_16bit?16:8);

    timeout = T8254_REF_CLOCK_HZ * 2UL;
    record_max = &record[MAX_RECORD];

    for (count=0;count < (sizeof(ess_tc_rates)/sizeof(ess_tc_rates[0]));count++) {
        /* 00 = single byte
         * 01 = demand 2-byte
         * 10 = demand 4-byte
         * 11 = "reserved" (we want to see what happens, doesn't appear to hurt the chipset any) */
        for (dma_xfer=0;dma_xfer <= 3;dma_xfer++) {
            if (ess_tc_rates[count] >= 128)
                expect = 795500UL / (256 - ess_tc_rates[count]);
            else
                expect = 397700UL / (128 - ess_tc_rates[count]);

            record_pos = record;

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

            printf("Starting test... tlen=%lu dmalen=%lu dma_xfer=%u\n",(unsigned long)tlen * (unsigned long)bytespersample,(unsigned long)sb_card->buffer_size,dma_xfer);

            sb_card->buffer_dma_started_length = tlen * (unsigned long)bytespersample;
            sb_card->buffer_dma_started = 0;

            sndsb_reset_dsp(sb_card);
            sndsb_write_dsp(sb_card,0xD1); /* speaker on */
            sndsb_write_dsp(sb_card,0x10); /* direct DAC reset to neutral output (0V) */
            sndsb_write_dsp(sb_card,0x80);
            sndsb_setup_dma(sb_card);

            time = 0;
            pd = d = (~0UL);

            {
                /* ESS 688/1869 chipset specific DSP playback.
                 * using this mode bypasses a lot of the Sound Blaster Pro emulation
                 * and restrictions and allows us to run up to 48KHz 16-bit stereo */
                unsigned short t16;

                _cli();

                /* clear IRQ */
                sndsb_interrupt_ack(sb_card,3);

                b = 0x00; /* DMA disable */
                b |= 0x00; /* no auto-init */
                b |= 0x00; /* [3]=DMA converter in ADC mode [1]=DMA read for ADC playback mode */
                sndsb_ess_write_controller(sb_card,0xB8,b);

                b = sndsb_ess_read_controller(sb_card,0xA8);
                b &= ~0xB; /* clear mono/stereo and record monitor (bits 3, 1, and 0) */
                b |= 2;     /* mono 10=mono 01=stereo */
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

                sndsb_ess_write_controller(sb_card,0xB9,dma_xfer);

                sndsb_ess_write_controller(sb_card,0xA1,ess_tc_rates[count]);

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
            }

            _cli();
            c = read_8254(T8254_TIMER_INTERRUPT_TICK);

            /* this begins playback */
            sndsb_ess_write_controller(sb_card,0xB8,b | 1);

            while (1) {
                /* up to 16 iterations.
                 * do not poll for IRQ changes, we have interrupts disabled here */
                iter = 16;
                do {
                    pd = d;
                    d = d8237_read_count(sb_card->dma8); /* counts DOWNWARD */
                } while (--iter != 0U && pd == d);

                if (d > tlen) bytes = tlen; /* terminal count */
                else bytes = tlen - d;

                pc = c;
                c = read_8254(T8254_TIMER_INTERRUPT_TICK);
                time += (unsigned long)((pc - c) & 0xFFFFU); /* remember: it counts DOWN. assumes full 16-bit count */

                if (pd != d) {
                    if (record_entry((uint16_t)bytes,time)) break; /* break if log is full. it will warn only once */
                }
                else if (time >= timeout) {
                    record_entry((uint16_t)bytes,time); /* may return 1 if this makes the log full. we break anyway */
                    break;
                }
            }
            _sti();

            sndsb_reset_dsp(sb_card);

            doubleprintf(" - Test at %luHz, %lu bytes, %s\n",expect,bytes * (unsigned long)bytespersample,dma_xfer_str[dma_xfer]);
            printf("Writing results... please wait\n"); /* The IDE-CF adapter setup in my old Pentium 133MHz is fairly slow at writing a 1MB text file */

            for (record_read=record;record_read!=record_pos;record_read++)
                fprintf(report_fp," >> POS %u, time %.6f\n",record_read->dma_pos * (unsigned int)bytespersample,(double)record_read->timer_pos / T8254_REF_CLOCK_HZ);

            fprintf(report_fp,"\n");
            fflush(report_fp);

            if (kbhit()) {
                if (getch() == 27) {
                    dma_xfer = 4;
                    count = 99;
                    break;
                }
            }
        }
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
	int sc_idx = -1;
    int i;

	if (!probe_8237()) {
		printf("WARNING: Cannot init 8237 DMA\n");
        return 1;
    }
	if (!probe_8259()) {
		printf("Cannot init 8259 PIC\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!init_sndsb()) {
		printf("Cannot init library\n");
		return 1;
	}

	/* it's up to us now to tell it certain minor things */
	sndsb_detect_virtualbox();		// whether or not we're running in VirtualBox
	/* sndsb now allows us to keep the EXE small by not referring to extra sound card support */
	sndsb_enable_sb16_support();		// SB16 support
	sndsb_enable_sc400_support();		// SC400 support
	sndsb_enable_ess_audiodrive_support();	// ESS AudioDrive support

    if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}

    if (find_isa_pnp_bios()) {
        int ret;
        char tmp[192];
        unsigned int j,nodesize=0;
        const char *whatis = NULL;
        unsigned char csn,node=0,numnodes=0xFF,data[192];

        memset(data,0,sizeof(data));
        if (isa_pnp_bios_get_pnp_isa_cfg(data) == 0) {
            struct isapnp_pnp_isa_cfg *nfo = (struct isapnp_pnp_isa_cfg*)data;
            isapnp_probe_next_csn = nfo->total_csn;
            isapnp_read_data = nfo->isa_pnp_port;
        }
        else {
            printf("  ISA PnP BIOS failed to return configuration info\n");
        }

        /* enumerate device nodes reported by the BIOS */
        if (isa_pnp_bios_number_of_sysdev_nodes(&numnodes,&nodesize) == 0 && numnodes != 0xFF && nodesize <= sizeof(devnode_raw)) {
            for (node=0;node != 0xFF;) {
                struct isa_pnp_device_node far *devn;
                unsigned char this_node;

                /* apparently, start with 0. call updates node to
                 * next node number, or 0xFF to signify end */
                this_node = node;
                if (isa_pnp_bios_get_sysdev_node(&node,devnode_raw,ISA_PNP_BIOS_GET_SYSDEV_NODE_CTRL_NOW) != 0) break;

                devn = (struct isa_pnp_device_node far*)devnode_raw;
                if (isa_pnp_is_sound_blaster_compatible_id(devn->product_id,&whatis)) {
                    isa_pnp_product_id_to_str(tmp,devn->product_id);
                    if ((ret = sndsb_try_isa_pnp_bios(devn->product_id,this_node,devn,sizeof(devnode_raw))) <= 0)
                        printf("ISA PnP BIOS: error %d for %s '%s'\n",ret,tmp,whatis);
                    else
                        printf("ISA PnP BIOS: found %s '%s'\n",tmp,whatis);
                }
            }
        }

        /* enumerate the ISA bus directly */
        if (isapnp_read_data != 0) {
            printf("Scanning ISA PnP devices...\n");
            for (csn=1;csn < 255;csn++) {
                isa_pnp_init_key();
                isa_pnp_wake_csn(csn);

                isa_pnp_write_address(0x06); /* CSN */
                if (isa_pnp_read_data() == csn) {
                    /* apparently doing this lets us read back the serial and vendor ID in addition to resource data */
                    /* if we don't, then we only read back the resource data */
                    isa_pnp_init_key();
                    isa_pnp_wake_csn(csn);

                    for (j=0;j < 9;j++) data[j] = isa_pnp_read_config();

                    if (isa_pnp_is_sound_blaster_compatible_id(*((uint32_t*)data),&whatis)) {
                        isa_pnp_product_id_to_str(tmp,*((uint32_t*)data));
                        if ((ret = sndsb_try_isa_pnp(*((uint32_t*)data),csn)) <= 0)
                            printf("ISA PnP: error %d for %s '%s'\n",ret,tmp,whatis);
                        else
                            printf("ISA PnP: found %s '%s'\n",tmp,whatis);
                    }
                }

                /* return back to "wait for key" state */
                isa_pnp_write_data_register(0x02,0x02);	/* bit 1: set -> return to Wait For Key state (or else a Pentium Pro system I own eventually locks up and hangs) */
            }
        }
    }

	/* Non-plug & play scan */
	if (sndsb_try_blaster_var() != NULL) {
		if (!sndsb_init_card(sndsb_card_blaster))
			sndsb_free_card(sndsb_card_blaster);
	}
    if (sndsb_try_base(0x220))
        printf("Also found one at 0x220\n");
    if (sndsb_try_base(0x240))
        printf("Also found one at 0x240\n");

    /* init card no longer probes the mixer */
    for (i=0;i < SNDSB_MAX_CARDS;i++) {
        struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
        if (cx->baseio == 0) continue;

        if (cx->irq < 0)
            sndsb_probe_irq_F2(cx);
        if (cx->irq < 0)
            sndsb_probe_irq_80(cx);
        if (cx->dma8 < 0)
            sndsb_probe_dma8_E2(cx);
        if (cx->dma8 < 0)
            sndsb_probe_dma8_14(cx);

		// having IRQ and DMA changes the ideal playback method and capabilities
		sndsb_update_capabilities(cx);
		sndsb_determine_ideal_dsp_play_method(cx);
	}

	if (sc_idx < 0) {
		int count=0;
		for (i=0;i < SNDSB_MAX_CARDS;i++) {
			const char *ess_str;
			const char *mixer_str;

			struct sndsb_ctx *cx = sndsb_index_to_ctx(i);
			if (cx->baseio == 0) continue;

#if !(TARGET_MSDOS == 16 && (defined(__TINY__) || defined(__SMALL__) || defined(__COMPACT__))) /* this is too much to cram into a small model EXE */
			mixer_str = sndsb_mixer_chip_str(cx->mixer_chip);
			ess_str = sndsb_ess_chipset_str(cx->ess_chipset);
#else
			mixer_str = "";
			ess_str = "";
#endif

			printf("  [%u] base=%X mpu=%X dma=%d dma16=%d irq=%d DSP=%u 1.XXAI=%u\n",
					i+1,cx->baseio,cx->mpuio,cx->dma8,cx->dma16,cx->irq,cx->dsp_ok,cx->dsp_autoinit_dma);
			printf("      MIXER=%u[%s] DSPv=%u.%u SC6600=%u OPL=%X GAME=%X AWE=%X\n",
					cx->mixer_ok,mixer_str,(unsigned int)cx->dsp_vmaj,(unsigned int)cx->dsp_vmin,
					cx->is_gallant_sc6600,cx->oplio,cx->gameio,cx->aweio);
			printf("      ESS=%u[%s] use=%u wss=%X OPL3SAx=%X\n",
					cx->ess_chipset,ess_str,cx->ess_extensions,cx->wssio,cx->opl3sax_controlio);
#ifdef ISAPNP
			if (cx->pnp_name != NULL) {
				isa_pnp_product_id_to_str(temp_str,cx->pnp_id);
				printf("      ISA PnP[%u]: %s %s\n",cx->pnp_csn,temp_str,cx->pnp_name);
			}
#endif
			printf("      '%s'\n",cx->dsp_copyright);

			count++;
		}
		if (count == 0) {
			printf("No cards found.\n");
			return 1;
		}
		printf("-----------\n");
		printf("Select the card you wish to test: "); fflush(stdout);
		i = getch();
		printf("\n");
		if (i == 27) return 0;
		if (i == 13 || i == 10) i = '1';
		sc_idx = i - '0';
	}

	if (sc_idx < 1 || sc_idx > SNDSB_MAX_CARDS) {
		printf("Sound card index out of range\n");
		return 1;
	}

	sb_card = &sndsb_card[sc_idx-1];
	if (sb_card->baseio == 0) {
		printf("No such card\n");
		return 1;
	}

	printf("Allocating sound buffer..."); fflush(stdout);

    write_8254_system_timer(0);

    printf("Test results will be written to TS_PD.TXT\n");

    report_fp = fopen("TS_PD.TXT","w");
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

    sb1_sc_play_test();
    sb2_sc_play_test();
    ess_sc_play_test();
    sb16_sc_play_test();

    wav_16bit = 1;
    sb_card->buffer_16bit = wav_16bit;
    realloc_dma_buffer();
	sndsb_assign_dma_buffer(sb_card,sb_dma);
    generate_1khz_sine16();

    ess_sc_play_test();
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

