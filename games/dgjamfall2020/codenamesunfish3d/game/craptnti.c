
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8237/8237.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/sndsb/sndsb.h>
#include <hw/adlib/adlib.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"
#include "craptn52.h"
#include "ldwavsn.h"

#include <hw/8042/8042.h>

static void woo_title_display(unsigned char *imgbuf,unsigned int w,unsigned int h,const char *path) {
    struct minipng_reader *rdr;
    unsigned char *dp,*sp;
    unsigned int i,j,c;

    rdr = woo_title_load_png(imgbuf,w,h,path);
    if (rdr == NULL) fatal("woo_title title %s",path);

    /* set the VGA palette first. this will deliberately cause a palette flash and corruption
     * of the old image's palette. furthermore we purposely draw it on screen slowly line by
     * line. Because Mr. Wooo. Sorcerer programming mistakes and slow inefficient code. */
    if (rdr->plte != NULL) {
        vga_palette_lseek(0);
        for (i=0;i < rdr->plte_count;i++)
            vga_palette_write(rdr->plte[i].red>>2,rdr->plte[i].green>>2,rdr->plte[i].blue>>2);
    }

    /* 4-pixel rendering. This code is still using VGA unchained 256-color mode. */
    for (i=0;i < h;i++) {
        sp = imgbuf + (i * w);

        for (c=0;c < 4;c++) {
            dp = vga_state.vga_graphics_ram + (i * 80u);
            vga_write_sequencer(0x02/*map mask*/,1u << c);
            for (j=c;j < w;j += 4) *dp++ = sp[j];
        }

        if ((i&7) == 6) {
            vga_wait_for_vsync();
            vga_wait_for_vsync_end();
        }
    }

    minipng_reader_close(&rdr);
}

void woo_title(void) {
    unsigned char *imgbuf;
    uint32_t now,next;
    int c;

    imgbuf = malloc(320u*200u); // 64000 bytes
    if (imgbuf == NULL) fatal("woo_title imgbuf NULL");

    /* If using Sound Blaster, allocate a DMA buffer to loop the "Wooo! Yeah!" from Action 52.
     * The WAV file is ~24KB, so allocating 28KB should be fine, along with the "Make your selection now" clip. */
    if (sound_blaster_ctx != NULL) {
        unsigned long srate;
        unsigned int channels,bits;
        unsigned long length;

        if (realloc_sound_blaster_dma(28u << 10u)) /* 28KB */
            fatal("Sound Blaster DMA alloc 28KB");

        /* load the WAV file into the DMA buffer, and set the buffer size for it to loop. */
        if (load_wav_into_buffer(&length,&srate,&channels,&bits,sound_blaster_dma->lin,sound_blaster_dma->length,"act52woo.wav") || bits < 8 || bits > 16 || channels < 1 || channels > 2)
            fatal("WAV file act52woo.wav len=%lu srate=%lu ch=%u bits=%u",length,srate,channels,bits);

        sound_blaster_stop_on_irq = 0;
        sound_blaster_ctx->force_single_cycle = 0; /* go ahead, loop */
        sound_blaster_ctx->buffer_irq_interval = length;
        sound_blaster_ctx->buffer_size = length;
        if (!sndsb_prepare_dsp_playback(sound_blaster_ctx,srate,channels>=2?1:0,bits>=16?1:0))
            fatal("sb prepare dsp");
        if (!sndsb_setup_dma(sound_blaster_ctx))
            fatal("sb dma setup");
        if (!sndsb_begin_dsp_playback(sound_blaster_ctx))
            fatal("sb dsp play");
    }

    /* as part of the gag, set the VGA mode X rendering to draw on active display.
     * furthermore the code is written to set palette, then draw the code with
     * deliberately poor performance. Going from one title image to another this
     * causes a palette flash inbetween. The visual gag here is that Mr. Wooo. Sorcerer
     * isn't the expert he thinks he is and makes some n00b programming mistakes like
     * that. */
    vga_cur_page = vga_next_page = VGA_PAGE_FIRST;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);

    woo_title_display(imgbuf,320,200,"cr52ti1.png");
    now = read_timer_counter();
    next = now + (120u * 6u);
    do {
        now = read_timer_counter();
        if (kbhit()) {
            c = getch();
            if (c == 27) {
                goto finishnow;
            }
            else if (c == ' ') {
                next = now;
            }
        }
    } while (now < next);

    woo_title_display(imgbuf,320,200,"cr52ti2.png");
    now = read_timer_counter();
    next = now + (120u * 8u);
    do {
        now = read_timer_counter();
        if (kbhit()) {
            c = getch();
            if (c == 27) {
                goto finishnow;
            }
            else if (c == ' ') {
                next = now;
            }
        }
    } while (now < next);

finishnow:
    /* "Make your selection, now" */
    if (sound_blaster_ctx != NULL) {
        unsigned long srate;
        unsigned int channels,bits;
        unsigned long length;

        sndsb_stop_dsp_playback(sound_blaster_ctx);

        if (realloc_sound_blaster_dma(28u << 10u)) /* 28KB */
            fatal("Sound Blaster DMA alloc 28KB");

        /* load the WAV file into the DMA buffer, and set the buffer size for it to loop. */
        if (load_wav_into_buffer(&length,&srate,&channels,&bits,sound_blaster_dma->lin,sound_blaster_dma->length,"act52sel.wav") || bits < 8 || bits > 16 || channels < 1 || channels > 2)
            fatal("WAV file act52sel.wav len=%lu srate=%lu ch=%u bits=%u",length,srate,channels,bits);

        /* make it play non-looping by disabling DSP/DMA autoinit, then set a flag telling our SB IRQ handler to stop and not continue playback. */
        sound_blaster_stop_on_irq = 1;
        sound_blaster_ctx->force_single_cycle = 1;
        sound_blaster_ctx->buffer_irq_interval = length;
        sound_blaster_ctx->buffer_size = length;
        if (!sndsb_prepare_dsp_playback(sound_blaster_ctx,srate,channels>=2?1:0,bits>=16?1:0))
            fatal("sb prepare dsp");
        if (!sndsb_setup_dma(sound_blaster_ctx))
            fatal("sb dma setup");
        if (!sndsb_begin_dsp_playback(sound_blaster_ctx))
            fatal("sb dsp play");

        /* leave it to run while going to the menu */
    }

    free(imgbuf);
}

