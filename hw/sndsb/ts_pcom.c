
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

void generate_1khz_sine16s(void) {
    unsigned int i,l;

    printf("Generating tone 16-bit stereo...\n");

    l = (unsigned int)sb_dma->length / 2U;
    for (i=0;i < l;i += 2) { // left channel only
        ((uint16_t FAR*)sb_dma->lin)[i+0] =
            (uint16_t)((int16_t)(sin(((double)i * 3.14159 * 2) / 100) * 16384));
        ((uint16_t FAR*)sb_dma->lin)[i+1] =
            0;
    }
}

void doubleprintf(const char *fmt,...) {
    va_list va;

    va_start(va,fmt);
    vsnprintf(ptmp,sizeof(ptmp),fmt,va);
    va_end(va);

    fputs(ptmp,stdout);
    fputs(ptmp,report_fp);
}

int common_sb_init(void) {
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
        if (i == 27) return 1;
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

    write_8254_system_timer(0);

    return 0;
}

