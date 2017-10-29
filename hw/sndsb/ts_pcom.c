
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

FILE*                            report_fp = NULL;

struct dma_8237_allocation*      sb_dma = NULL; /* DMA buffer */

struct sndsb_ctx*                sb_card = NULL;

unsigned char far                devnode_raw[4096];

uint32_t                         buffer_limit = 0xF000UL;

char                             ptmp[256];

unsigned int                     wav_sample_rate = 4000;
unsigned char                    wav_stereo = 0;
unsigned char                    wav_16bit = 0;

unsigned char                    old_irq_masked = 0;
void                             (interrupt *old_irq)() = NULL;

void free_dma_buffer() {
    if (sb_dma != NULL) {
        dma_8237_free_buffer(sb_dma);
        sb_dma = NULL;
    }
}

void interrupt sb_irq() {
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

void realloc_dma_buffer() {
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

void generate_1khz_sine_adpcm2(void) {
    unsigned int i,c,l,b;

    printf("Generating tone ADPCM 2-bit...\n");

    l = (unsigned int)sb_dma->length;

    /* reference */
    for (i=0;i < 1;i++)
        sb_dma->lin[i] =
            (unsigned char)((sin(((double)i * 3.14159 * 2) / 100) * 64) + 128);

    /* ADPCM compression */
    sndsb_encode_adpcm_set_reference(sb_dma->lin[0],ADPCM_2BIT);
    for (c=i;i < l;i++,c += 4) {
        b  = sndsb_encode_adpcm_2bit((unsigned char)((sin(((double)(c+0) * 3.14159 * 2) / 100) * 64) + 128)) << 6;
        b += sndsb_encode_adpcm_2bit((unsigned char)((sin(((double)(c+1) * 3.14159 * 2) / 100) * 64) + 128)) << 4;
        b += sndsb_encode_adpcm_2bit((unsigned char)((sin(((double)(c+2) * 3.14159 * 2) / 100) * 64) + 128)) << 2;
        b += sndsb_encode_adpcm_2bit((unsigned char)((sin(((double)(c+3) * 3.14159 * 2) / 100) * 64) + 128)) << 0;
        sb_dma->lin[i] = b;
    }
}

void generate_1khz_sine_adpcm26(void) {
    unsigned int i,c,l,b;

    printf("Generating tone ADPCM 2.6-bit...\n");

    l = (unsigned int)sb_dma->length;

    /* reference */
    for (i=0;i < 1;i++)
        sb_dma->lin[i] =
            (unsigned char)((sin(((double)i * 3.14159 * 2) / 100) * 64) + 128);

    /* ADPCM compression */
    sndsb_encode_adpcm_set_reference(sb_dma->lin[0],ADPCM_2_6BIT);
    for (c=i;i < l;i++,c += 3) {
        b  = sndsb_encode_adpcm_2_6bit((unsigned char)((sin(((double)(c+0) * 3.14159 * 2) / 100) * 64) + 128),0) << 5;
        b += sndsb_encode_adpcm_2_6bit((unsigned char)((sin(((double)(c+1) * 3.14159 * 2) / 100) * 64) + 128),0) << 2;
        b += sndsb_encode_adpcm_2_6bit((unsigned char)((sin(((double)(c+2) * 3.14159 * 2) / 100) * 64) + 128),1) >> 1;
        sb_dma->lin[i] = b;
    }
}

void generate_1khz_sine_adpcm4(void) {
    unsigned int i,c,l,b;

    printf("Generating tone ADPCM 4-bit...\n");

    l = (unsigned int)sb_dma->length;

    /* reference */
    for (i=0;i < 1;i++)
        sb_dma->lin[i] =
            (unsigned char)((sin(((double)i * 3.14159 * 2) / 100) * 64) + 128);

    /* ADPCM compression */
    sndsb_encode_adpcm_set_reference(sb_dma->lin[0],ADPCM_4BIT);
    for (c=i;i < l;i++,c += 2) {
        b  = sndsb_encode_adpcm_4bit((unsigned char)((sin(((double)(c+0) * 3.14159 * 2) / 100) * 64) + 128)) << 4;
        b += sndsb_encode_adpcm_4bit((unsigned char)((sin(((double)(c+1) * 3.14159 * 2) / 100) * 64) + 128));
        sb_dma->lin[i] = b;
    }
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

    printf("Generating tone 16-bit...\n");

    l = (unsigned int)sb_dma->length / 2U;
    for (i=0;i < l;i++)
        ((uint16_t FAR*)sb_dma->lin)[i] =
            (uint16_t)((int16_t)(sin(((double)i * 3.14159 * 2) / 100) * 16384));
}

void doubleprintf(const char *fmt,...) {
    va_list va;

    va_start(va,fmt);
    vsnprintf(ptmp,sizeof(ptmp),fmt,va);
    va_end(va);

    fputs(ptmp,stdout);
    fputs(ptmp,report_fp);
}

