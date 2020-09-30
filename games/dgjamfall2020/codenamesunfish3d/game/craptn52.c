
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
#include "ldwavsn.h"

#include <hw/8042/8042.h>

unsigned char FAR*                  vga_8x8_font_ptr;

struct dma_8237_allocation*         sound_blaster_dma = NULL; /* DMA buffer */
struct sndsb_ctx*                   sound_blaster_ctx = NULL;

unsigned char           sound_blaster_stop_on_irq = 0;
unsigned char           sound_blaster_irq_hook = 0;
unsigned char           sound_blaster_old_irq_masked = 0; /* very important! If the IRQ was masked prior to running this program there's probably a good reason */
void                    (interrupt *sound_blaster_old_irq)() = NULL;

void free_sound_blaster_dma(void) {
    if (sound_blaster_dma != NULL) {
        sndsb_assign_dma_buffer(sound_blaster_ctx,NULL);
        dma_8237_free_buffer(sound_blaster_dma);
        sound_blaster_dma = NULL;
    }
}

int realloc_sound_blaster_dma(const unsigned buffer_size) {
    uint32_t choice;
    int8_t ch;

    if (sound_blaster_dma != NULL) {
        if (sound_blaster_dma->length >= buffer_size)
            return 0;
    }

    free_sound_blaster_dma();

    ch = sndsb_dsp_playback_will_use_dma_channel(sound_blaster_ctx,22050,0/*mono*/,0/*8-bit*/);

    if (ch >= 4)
        choice = sndsb_recommended_16bit_dma_buffer_size(sound_blaster_ctx,buffer_size);
    else
        choice = sndsb_recommended_dma_buffer_size(sound_blaster_ctx,buffer_size);

    if (ch >= 4)
        sound_blaster_dma = dma_8237_alloc_buffer_dw(choice,16);
    else
        sound_blaster_dma = dma_8237_alloc_buffer_dw(choice,8);

    if (sound_blaster_dma == NULL)
        return 1;

    if (!sndsb_assign_dma_buffer(sound_blaster_ctx,sound_blaster_dma))
        return 1;

    return 0;
}

void interrupt sound_blaster_irq() {
	unsigned char c;

	sound_blaster_ctx->irq_counter++;

	/* ack soundblaster DSP if DSP was the cause of the interrupt */
	/* NTS: Experience says if you ack the wrong event on DSP 4.xx it
	   will just re-fire the IRQ until you ack it correctly...
	   or until your program crashes from stack overflow, whichever
	   comes first */
	c = sndsb_interrupt_reason(sound_blaster_ctx);
	sndsb_interrupt_ack(sound_blaster_ctx,c);

    if (!sound_blaster_stop_on_irq) {
        /* FIXME: The sndsb library should NOT do anything in
           send_buffer_again() if it knows playback has not started! */
        /* for non-auto-init modes, start another buffer */
        sndsb_irq_continue(sound_blaster_ctx,c);
    }

	/* NTS: we assume that if the IRQ was masked when we took it, that we must not
	 *      chain to the previous IRQ handler. This is very important considering
	 *      that on most DOS systems an IRQ is masked for a very good reason---the
	 *      interrupt handler doesn't exist! In fact, the IRQ vector could easily
	 *      be unitialized or 0000:0000 for it! CALLing to that address is obviously
	 *      not advised! */
	if (sound_blaster_old_irq_masked || sound_blaster_old_irq == NULL) {
		/* ack the interrupt ourself, do not chain */
		if (sound_blaster_ctx->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
	else {
		/* chain to the previous IRQ, who will acknowledge the interrupt */
		sound_blaster_old_irq();
	}
}

void sound_blaster_unhook_irq(void) {
    if (sound_blaster_irq_hook && sound_blaster_ctx != NULL) {
        if (sound_blaster_ctx->irq >= 0) {
            /* If the IRQ was masked on hooking, then mask the IRQ again */
            if (sound_blaster_old_irq_masked)
                p8259_mask(sound_blaster_ctx->irq);

            /* Restore the old IRQ handler */
            _dos_setvect(irq2int(sound_blaster_ctx->irq),sound_blaster_old_irq);
        }

        sound_blaster_old_irq = NULL;
        sound_blaster_irq_hook = 0;
    }
}

void sound_blaster_hook_irq(void) {
    if (!sound_blaster_irq_hook && sound_blaster_ctx != NULL) {
        if (sound_blaster_ctx->irq >= 0) {
            /* If the IRQ was masked on entry, there's probably a good reason for it, such as
             * a NULL vector, a BIOS (or DOSBox) with just an IRET instruction that doesn't
             * acknowledge the interrupt, or perhaps some junk. Whatever the reason, take it
             * as a sign not to chain to the previous interrupt handler. */
            sound_blaster_old_irq_masked = p8259_is_masked(sound_blaster_ctx->irq);
            if (vector_is_iret(irq2int(sound_blaster_ctx->irq)))
                sound_blaster_old_irq_masked = 1;

            /* hook the IRQ, install our own, then unmask the IRQ */
            sound_blaster_irq_hook = 1;
            sound_blaster_old_irq = _dos_getvect(irq2int(sound_blaster_ctx->irq));
            _dos_setvect(irq2int(sound_blaster_ctx->irq),sound_blaster_irq);
            p8259_unmask(sound_blaster_ctx->irq);
        }
    }
}

void sound_blaster_stop_playback(void) {
    if (sound_blaster_ctx != NULL)
        sndsb_stop_dsp_playback(sound_blaster_ctx);
}

void my_unhook_irq(void) {
    sound_blaster_stop_playback();
    sound_blaster_unhook_irq();
}

void gen_res_free(void) {
//    seq_com_cleanup();
//    sin2048fps16_free();
//    font_bmp_free(&arial_small);
//    font_bmp_free(&arial_medium);
//    font_bmp_free(&arial_large);
//    dumbpack_close(&sorc_pack);

    sound_blaster_stop_playback();
    sound_blaster_unhook_irq();
    free_sound_blaster_dma();
    if (sound_blaster_ctx != NULL) {
        sndsb_free_card(sound_blaster_ctx);
        sound_blaster_ctx = NULL;
    }
}

static struct minipng_reader *woo_title_load_png(unsigned char *buf,unsigned int w,unsigned int h,const char *path) {
    struct minipng_reader *rdr;
    unsigned int y;

    if ((rdr=minipng_reader_open(path)) == NULL)
        return NULL;

    if (minipng_reader_parse_head(rdr) || rdr->plte == NULL || rdr->plte_count == 0 || rdr->ihdr.width != w || rdr->ihdr.height != h) {
        minipng_reader_close(&rdr);
        return NULL;
    }

    for (y=0;y < h;y++) {
        unsigned char *imgptr = buf + (y * w);

        if (minipng_reader_read_idat(rdr,imgptr,1) != 1) { /* pad byte */
            minipng_reader_close(&rdr);
            return NULL;
        }

        if (rdr->ihdr.bit_depth == 8) {
            if (minipng_reader_read_idat(rdr,imgptr,w) != w) { /* row */
                minipng_reader_close(&rdr);
                return NULL;
            }
        }
        else if (rdr->ihdr.bit_depth == 4) {
            if (minipng_reader_read_idat(rdr,imgptr,(w+1u)>>1u) != ((w+1u)>>1u)) { /* row */
                minipng_reader_close(&rdr);
                return NULL;
            }
            minipng_expand4to8(imgptr,w);
        }
    }

    return rdr;
}

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

#define TOTAL_GAMES                 52
#define GAMES_PER_COLUMN            9
#define GAMES_PER_PAGE              18
#define TOTAL_PAGES                 ((TOTAL_GAMES+GAMES_PER_PAGE-1)/GAMES_PER_PAGE)

struct menu_entry_t {
    const char*                     title;
    const char*                     longtitle;
};

static struct menu_entry_t menu_entries[TOTAL_GAMES] = {
    {"Entry0",              "Entry #0"},                                // 0
    {"Entry1",              "Entry #1"},                                // 1
    {"Entry2",              "Entry #2"},                                // 2
    {"Entry3",              "Entry #3"},                                // 3
    {"Entry4",              "Entry #4"},                                // 4
    {"Entry5",              "Entry #5"},                                // 5
    {"Entry6",              "Entry #6"},                                // 6
    {"Entry7",              "Entry #7"},                                // 7
    {"Entry8",              "Entry #8"},                                // 8
    {"Entry9",              "Entry #9"},                                // 9
    {"Entry10",             "Entry #10"},                               // 10
    {"Entry11",             "Entry #11"},                               // 11
    {"Entry12",             "Entry #12"},                               // 12
    {"Entry13",             "Entry #13"},                               // 13
    {"Entry14",             "Entry #14"},                               // 14
    {"Entry15",             "Entry #15"},                               // 15
    {"Entry16",             "Entry #16"},                               // 16
    {"Entry17",             "Entry #17"},                               // 17
    {"Entry18",             "Entry #18"},                               // 18
    {"Entry19",             "Entry #19"},                               // 19
    {"Entry20",             "Entry #20"},                               // 20
    {"Entry21",             "Entry #21"},                               // 21
    {"Entry22",             "Entry #22"},                               // 22
    {"Entry23",             "Entry #23"},                               // 23
    {"Entry24",             "Entry #24"},                               // 24
    {"Entry25",             "Entry #25"},                               // 25
    {"Entry26",             "Entry #26"},                               // 26
    {"Entry27",             "Entry #27"},                               // 27
    {"Entry28",             "Entry #28"},                               // 28
    {"Entry29",             "Entry #29"},                               // 29
    {"Entry30",             "Entry #30"},                               // 30
    {"Entry31",             "Entry #31"},                               // 31
    {"Entry32",             "Entry #32"},                               // 32
    {"Entry33",             "Entry #33"},                               // 33
    {"Entry34",             "Entry #34"},                               // 34
    {"Entry35",             "Entry #35"},                               // 35
    {"Entry36",             "Entry #36"},                               // 36
    {"Entry37",             "Entry #37"},                               // 37
    {"Entry38",             "Entry #38"},                               // 38
    {"Entry39",             "Entry #39"},                               // 39
    {"Entry40",             "Entry #40"},                               // 40
    {"Entry41",             "Entry #41"},                               // 41
    {"Entry42",             "Entry #42"},                               // 42
    {"Entry43",             "Entry #43"},                               // 43
    {"Entry44",             "Entry #44"},                               // 44
    {"Entry45",             "Entry #45"},                               // 45
    {"Entry46",             "Entry #46"},                               // 46
    {"Entry47",             "Entry #47"},                               // 47
    {"Entry48",             "Entry #48"},                               // 48
    {"Entry49",             "Entry #49"},                               // 49
    {"Entry50",             "Entry #50"},                               // 50
    {"Entry51",             "Entry #51"}                                // 51
};                                                                      //=52

void woo_menu_item_coord(unsigned int *x,unsigned int *y,unsigned int i) {
    *x = (40u/4u) + ((140u/4u) * (i/GAMES_PER_COLUMN));
    *y = 40u + ((i%GAMES_PER_COLUMN) * 14u);
}

static const uint8_t vrev4[16] = {
    0x0,    // 0000 0000
    0x8,    // 0001 1000
    0x4,    // 0010 0100
    0xC,    // 0011 1100
    0x2,    // 0100 0010
    0xA,    // 0101 1010
    0x6,    // 0110 0110
    0xE,    // 0111 1110
    0x1,    // 1000 0001
    0x9,    // 1001 1001
    0x5,    // 1010 0101
    0xD,    // 1011 1101
    0x3,    // 1100 0011
    0xB,    // 1101 1011
    0x7,    // 1110 0111
    0xF     // 1111 1111
};

void woo_menu_item_draw_char(unsigned int o,unsigned char c,unsigned char color) {
    unsigned char FAR *fbmp = vga_8x8_font_ptr + ((unsigned int)c * 8u);
    unsigned char cb;
    unsigned int i;

    for (i=0;i < 8;i++) {
        cb = fbmp[i];

        vga_write_sequencer(0x02/*map mask*/,vrev4[(cb >> 4u) & 0xFu]);
        vga_state.vga_graphics_ram[o+0] = color;

        vga_write_sequencer(0x02/*map mask*/,vrev4[(cb >> 0u) & 0xFu]);
        vga_state.vga_graphics_ram[o+1] = color;

        o += 80u;
    }
}

void woo_menu_item_drawstr(unsigned int x,unsigned int y,const char *str,unsigned char color) {
    /* 'x' is in units of 4 pixels because unchained 256-color mode */
    unsigned int o;
    char c;

    o = (y * 80u) + x;
    while ((c=(*str++)) != 0) {
        woo_menu_item_draw_char(o,(unsigned char)c,color);
        o += 2u;
    }
}

void woo_menu_item_draw(unsigned int x,unsigned int y,unsigned int i,unsigned int sel) {
    /* 'x' is in units of 4 pixels because unchained 256-color mode */
    const char *str = "";
    unsigned char color;
    unsigned int o;
    char c;

    color = (i == sel) ? 7 : 0;

    o = (y * 80u) + x - 2u;
    woo_menu_item_draw_char(o,(unsigned char)'>',color);

    color = (i == sel) ? 15 : 7;

    o = (y * 80u) + x;
    if (i < TOTAL_GAMES)
        str = menu_entries[i].title;

    while ((c=(*str++)) != 0) {
        woo_menu_item_draw_char(o,(unsigned char)c,color);
        o += 2u;
    }
}

int woo_menu(void) {
    unsigned char page = 0,npage;
    unsigned char redraw = 7;
    unsigned char done = 0;
    unsigned int i,x,y;
    int psel = -1,sel = 0,c;

    /* use INT 16h here */
    restore_keyboard_irq();

    /* palette */
    vga_palette_lseek(0);
    vga_palette_write(0,0,0);
    vga_palette_lseek(7);
    vga_palette_write(22,35,35);
    vga_palette_lseek(15);
    vga_palette_write(63,63,63);

    vga_palette_lseek(1);
    vga_palette_write(15,2,2);
    vga_palette_lseek(2);
    vga_palette_write(15,15,15);

    /* again, no page flipping, drawing directly on screen */ 
    vga_cur_page = vga_next_page = VGA_PAGE_FIRST;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);

    page = (unsigned int)sel / (unsigned int)GAMES_PER_PAGE;

    while (!done) {
        if (redraw) {
            if (redraw & 1u) {
                npage = (unsigned int)sel / (unsigned int)GAMES_PER_PAGE;
                if (page != npage) {
                    page = npage;
                    redraw |= 7u;
                }
            }

            if (redraw & 4u) { // redraw background
                vga_write_sequencer(0x02/*map mask*/,0xFu);

                vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*0u),0x0000,((320u/4u)*200u)/2u);
                vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*0u),0x0101,((320u/4u)*31u)/2u);
                vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*31u),0x0202,((320u/4u)*1u)/2u);
                vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*169u),0x0202,((320u/4u)*1u)/2u);
                vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*170u),0x0101,((320u/4u)*30u)/2u);

                redraw |= 3u; // which means redraw all menu items
            }

            if (redraw & 2u) { // redraw all menu items
                for (i=0;i < GAMES_PER_PAGE;i++) {
                    woo_menu_item_coord(&x,&y,i);
                    woo_menu_item_draw(x,y,i+(page*GAMES_PER_PAGE),sel);
                }
            }
            else if (redraw & 1u) { // redraw active menu item
                if (psel != sel) {
                    woo_menu_item_coord(&x,&y,psel - (page * GAMES_PER_PAGE));
                    woo_menu_item_draw(x,y,psel,sel);
                }

                woo_menu_item_coord(&x,&y,sel - (page * GAMES_PER_PAGE));
                woo_menu_item_draw(x,y,sel,sel);
            }

            redraw = 0;
            psel = sel;
        }

        c = getch();
        if (c == 0) c = getch() << 8; /* extended IBM key code */
        if (c == 27) {
            sel = -1;
            break;
        }
        else if (c == 13 || c == ' ') { // ENTER key or SPACEBAR
            const char *title = "?";
            size_t titlelen;

            /* we take over the screen, signal redraw in case we return to menu */
            redraw |= 7u;

            vga_write_sequencer(0x02/*map mask*/,0xFu);
            vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*0u),0x0000,((320u/4u)*200u)/2u);

            if (sel >= 0 && sel < TOTAL_GAMES)
                title = menu_entries[sel].longtitle;

            titlelen = strlen(title);
            woo_menu_item_drawstr((320u-(titlelen*8u))/2u/4u,60,title,7);

            woo_menu_item_drawstr(108u/4u,90,"ENTER to start",7);
            woo_menu_item_drawstr(80u/4u,106,"ESC to return to menu",7);

            while (1) {
                c = getch();
                if (c == 0) c = getch() << 8; /* extended IBM key code */

                if (c == 13/*ENTER*/ || c == 27/*ESC*/) break;
            }

            if (c == 13)
                break;
        }
        else if (c == 0x4800) { // up arrow
            if (sel > 0)
                sel--;
            else
                sel = TOTAL_GAMES - 1;

            redraw |= 1u;
        }
        else if (c == 0x5000) { // down arrow
            sel++;
            if (sel >= TOTAL_GAMES)
                sel = 0;

            redraw |= 1u;
        }
        else if (c == 0x4B00) { // left arrow
            if (sel >= GAMES_PER_COLUMN)
                sel -= GAMES_PER_COLUMN;
            else {
                sel += TOTAL_GAMES + GAMES_PER_COLUMN - (TOTAL_GAMES%GAMES_PER_COLUMN);
                while (sel >= TOTAL_GAMES)
                    sel -= GAMES_PER_COLUMN;
            }

            redraw |= 1u;
        }
        else if (c == 0x4D00) { // down arrow
            sel += GAMES_PER_COLUMN;
            if (sel >= TOTAL_GAMES)
                sel %= GAMES_PER_COLUMN;

            redraw |= 1u;
        }
    }

    return sel;
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

/* Sound Blaster detection using hw/sndsb */
void detect_sound_blaster(void) {
    struct sndsb_ctx* ctx;

    /* First allow the user to control our detection with SET BLASTER=... in the environment block.
     * Since DOSBox/DOSBox-X usually sets BLASTER this means we'll use whatever I/O port, IRQ, and DMA
     * they assigned in dosbox.conf as well. */
    if (sndsb_index_to_ctx(0)->baseio == 0/*NTS: currently does not return NULL*/ && sndsb_try_blaster_var() != NULL) {
        if (!sndsb_init_card(sndsb_card_blaster))
            sndsb_free_card(sndsb_card_blaster);
    }

    /* Otherwise, try the usual port 220h and port 240h most Sound Blaster cards are configured on,
     * but only if we didn't get anything with SET BLASTER=...  Port 220h is VERY COMMON, Port 240h
     * is much less common. */
    if (sndsb_index_to_ctx(0)->baseio == 0/*NTS: currently does not return NULL*/)
        sndsb_try_base(0x220);
    if (sndsb_index_to_ctx(0)->baseio == 0/*NTS: currently does not return NULL*/)
        sndsb_try_base(0x240);

    /* Stop here if none detected */
    if ((ctx=sndsb_index_to_ctx(0))->baseio == 0/*NTS: currently does not return NULL*/)
        return;

    printf("Possible Sound Blaster detected at I/O port %xh\n",ctx->baseio);

    /* Autodetect the IRQ and DMA if not already obtained from SET BLASTER... */
    if (ctx->irq < 0)
        sndsb_probe_irq_F2(ctx);
    if (ctx->irq < 0)
        sndsb_probe_irq_80(ctx);
    if (ctx->dma8 < 0)
        sndsb_probe_dma8_E2(ctx);
    if (ctx->dma8 < 0)
        sndsb_probe_dma8_14(ctx);

    /* No IRQ/DMA, no sound. Not doing Goldplay or Direct DAC in *this* game, sorry */
    if (ctx->irq < 0 || ctx->dma8 < 0)
        return;

    /* Check card capabilities */
    sndsb_update_capabilities(ctx);
    sndsb_determine_ideal_dsp_play_method(ctx);

    /* Ok, accept */
    sound_blaster_ctx = ctx;

    printf("Found Sound Blaster at %xh IRQ %d DMA %d\n",sound_blaster_ctx->baseio,sound_blaster_ctx->irq,sound_blaster_ctx->dma8);

    sound_blaster_hook_irq();

    t8254_wait(t8254_us2ticks(1000000)); /* 1 second */
}

int main(int argc,char **argv) {
    unsigned char done;
    int sel;

    (void)argc;
    (void)argv;

    probe_dos();
	cpu_probe();
    if (cpu_basic_level < 3) {
        printf("This game requires a 386 or higher\n");
        return 1;
    }

	if (!probe_8237()) {
		printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("No 8254 detected.\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("No 8259 detected.\n");
		return 1;
	}
    if (!probe_vga()) {
        printf("VGA probe failed.\n");
        return 1;
    }
    if (!(vga_state.vga_flags & VGA_IS_VGA)) {
        printf("This game requires VGA\n");
        return 1;
    }
    if (!init_sndsb()) {
        printf("Sound Blaster lib init fail.\n");
        return 1;
    }

    /* as part of the gag, we use the VGA 8x8 BIOS font instead of the nice Arial font */
    vga_8x8_font_ptr = (unsigned char FAR*)_dos_getvect(0x43); /* Character table (8x8 chars 0x00-0x7F) [http://www.ctyme.com/intr/rb-6169.htm] */

    other_unhook_irq = my_unhook_irq;

    write_8254_system_timer(0);

    detect_keyboard();
    detect_sound_blaster();

#if TARGET_MSDOS == 16
# if 0 // not using it yet
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support
# endif
#endif

    init_timer_irq();
    init_vga256unchained();

    woo_title();

    done = 0;
    while (!done) {
        sel = woo_menu();
        switch (sel) {
            case -1:
                done = 1;
                break;
            default:
                fatal("Unknown menu selection %u",sel);
                break;
        }
    }

    gen_res_free();
    check_heap();
    unhook_irqs();
    restore_text_mode();

    //debug
    dbg_heap_list();

    return 0;
}

