
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
#include "ldwavsn.h"

#include <hw/8042/8042.h>

/* alternate page offsets big enough for a 352x232 mode */
#define VGA_GAME_PAGE_FIRST         0x0000
#define VGA_GAME_PAGE_SECOND        0x5000

void game_normal_setup(void);

static inline int use_adlib() {
    return (adlib_fm_voices != 0u);
}

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

void game_spriteimg_free(void);

void gen_res_free(void) {
    game_spriteimg_free();
    sin2048fps16_free();

//    seq_com_cleanup();
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

/* As part of the Action 52 theme, short and long names do not necessarily match or even spell the same */
static struct menu_entry_t menu_entries[TOTAL_GAMES] = {
    {"SmilWar",             "Smiley Wars"},                             // 0
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
    unsigned int trigger_voice = 0;
    const unsigned char sel_voice = 1;
    const unsigned char nav_voice = 0;
    uint32_t now;

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

    /* border color (overscan) */
    vga_write_AC(0x11 | VGA_AC_ENABLE,0x00);

    /* again, no page flipping, drawing directly on screen */ 
    vga_cur_page = vga_next_page = VGA_PAGE_FIRST;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);

    /* setup OPL for menu sound effects */
    if (use_adlib()) {
        struct adlib_fm_operator *f;

        f = &adlib_fm[nav_voice].mod;
        f->mod_multiple = 7;
        f->total_level = 47;
        f->attack_rate = 12;
        f->decay_rate = 7;
        f->sustain_level = 6;
        f->release_rate = 8;
        f->f_number = 580; /* 440Hz middle 'A' */
        f->octave = 4;
        f->key_on = 0;

        f = &adlib_fm[nav_voice].car;
        f->mod_multiple = 2;
        f->total_level = 47;
        f->attack_rate = 15;
        f->decay_rate = 6;
        f->sustain_level = 6;
        f->release_rate = 5;
        f->f_number = 0;
        f->octave = 0;
        f->key_on = 0;

        adlib_fm[sel_voice] = adlib_fm[nav_voice];
        adlib_fm[sel_voice].mod.octave = 5;

        adlib_apply_all();
    }

    page = (unsigned int)sel / (unsigned int)GAMES_PER_PAGE;

    while (!done) {
        now = read_timer_counter();

        if (use_adlib()) {
            if (trigger_voice & (1u << 0u)) {
                adlib_fm[nav_voice].mod.key_on = 0;
                adlib_update_groupA0(nav_voice,&adlib_fm[nav_voice]);
                adlib_fm[nav_voice].mod.key_on = 1;
                adlib_update_groupA0(nav_voice,&adlib_fm[nav_voice]);
            }

            trigger_voice = 0;
        }

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

        if (kbhit()) {
            c = getch();
            if (c == 0) c = getch() << 8; /* extended IBM key code */
        }
        else {
            c = 0;
        }

        if (c == 27) {
            sel = -1;
            break;
        }
        else if (c == 13 || c == ' ') { // ENTER key or SPACEBAR
            const char *title = "?";
            size_t titlelen;

            if (use_adlib()) {
                adlib_fm[sel_voice].mod.key_on = 0;
                adlib_update_groupA0(nav_voice,&adlib_fm[sel_voice]);
                adlib_fm[sel_voice].mod.key_on = 1;
                adlib_update_groupA0(nav_voice,&adlib_fm[sel_voice]);
            }

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

            if (use_adlib()) {
                adlib_fm[sel_voice].mod.key_on = 0;
                adlib_update_groupA0(nav_voice,&adlib_fm[sel_voice]);
                trigger_voice |= (1u << 0u);
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
            trigger_voice |= (1u << 0u);
        }
        else if (c == 0x5000) { // down arrow
            sel++;
            if (sel >= TOTAL_GAMES)
                sel = 0;

            redraw |= 1u;
            trigger_voice |= (1u << 0u);
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
            trigger_voice |= (1u << 0u);
        }
        else if (c == 0x4D00) { // down arrow
            sel += GAMES_PER_COLUMN;
            if (sel >= TOTAL_GAMES)
                sel %= GAMES_PER_COLUMN;

            redraw |= 1u;
            trigger_voice |= (1u << 0u);
        }
    }

    adlib_fm[nav_voice].mod.key_on = 0;
    adlib_update_groupA0(nav_voice,&adlib_fm[nav_voice]);
    adlib_fm[sel_voice].mod.key_on = 0;
    adlib_update_groupA0(sel_voice,&adlib_fm[sel_voice]);

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

void load_def_pal() {
    unsigned int i;
    int fd;

    fd = open("cr52pal.pal",O_RDONLY|O_BINARY);
    if (fd >= 0) {
        read(fd,common_tmp_small,216*3);
        close(fd);

        vga_palette_lseek(0);
        for (i=0;i < 216;i++) vga_palette_write(common_tmp_small[i*3+0]>>2u,common_tmp_small[i*3+1]>>2u,common_tmp_small[i*3+2]>>2u);
    }

    /* border color (overscan) */
    vga_write_AC(0x11 | VGA_AC_ENABLE,215);
}

#define NUM_TILES           256
#define TILES_VRAM_OFFSET   0xC000              /* enough for 4*16*256 */

#define GAME_VSCROLL        (1u << 0u)
#define GAME_HSCROLL        (1u << 1u)

#define NUM_SPRITEIMG       64

struct game_spriteimg {
    struct vrl_image        vrl;
};

struct game_spriteimg       game_spriteimg[NUM_SPRITEIMG];

#define NUM_SPRITES         32

#define GAME_SPRITE_VISIBLE (1u << 0u)
#define GAME_SPRITE_REDRAW  (1u << 1u)
#define GAME_SPRITE_HFLIP   (1u << 2u)

struct game_sprite {
    /* last drawn 2 frames ago (for erasure). 2 frame delay because page flipping. */
    unsigned int            ox,oy;
    unsigned int            ow,oh;
    /* last drawn 1 frame ago */
    unsigned int            px,py;
    unsigned int            pw,ph;
    /* where to draw */
    unsigned int            x,y;
    unsigned int            w,h;
    /* what to draw */
    unsigned char           spriteimg;
    /* other */
    unsigned char           flags;
};

unsigned char               game_sprite_max = 0;
struct game_sprite          game_sprite[NUM_SPRITES];

#define GAME_TILEMAP_DISPWIDTH  22
#define GAME_TILEMAP_WIDTH      ((22u*2u)+1u)
#define GAME_TILEMAP_HEIGHT     ((14u*2u)+1u)

unsigned char*              game_tilemap = NULL;

void game_sprite_hide(unsigned i) {
    if (i >= NUM_SPRITES)
        fatal("spriteimgset out of range");

    game_sprite[i].flags &= ~GAME_SPRITE_VISIBLE;
    game_sprite[i].flags |=  GAME_SPRITE_REDRAW;
}

void game_sprite_show(unsigned i) {
    if (i >= NUM_SPRITES)
        fatal("spriteimgset out of range");

    game_sprite[i].flags |=  GAME_SPRITE_VISIBLE;
    game_sprite[i].flags |=  GAME_SPRITE_REDRAW;
}

/* does not change position */
void game_sprite_imgset(unsigned i,unsigned img) {
    if (i >= NUM_SPRITES || img >= NUM_SPRITEIMG)
        fatal("spriteimgset out of range");

    game_sprite[i].spriteimg = img;
    if (game_spriteimg[img].vrl.buffer != NULL && game_spriteimg[img].vrl.vrl_header != NULL) {
        game_sprite[i].h = game_spriteimg[img].vrl.vrl_header->height;
        game_sprite[i].w = game_spriteimg[img].vrl.vrl_header->width;
    }
    else {
        game_sprite[i].h = game_sprite[i].w = 0;
    }

    game_sprite[i].flags |= GAME_SPRITE_REDRAW;
    game_sprite[i].flags |= GAME_SPRITE_VISIBLE;

    if (game_sprite_max < (i+1u))
        game_sprite_max = (i+1u);
}

void game_sprite_position(unsigned i,unsigned int x,unsigned int y) {
    if (i >= NUM_SPRITES)
        fatal("spriteimgpos out of range");

    game_sprite[i].x = x;
    game_sprite[i].y = y;
    game_sprite[i].flags |= GAME_SPRITE_REDRAW;
}

void game_sprite_clear(void) {
    game_sprite_max = 0;
}

void game_sprite_init(void) {
    game_sprite_clear();
    memset(game_sprite,0,sizeof(game_sprite));

    if (game_tilemap == NULL) {
        if ((game_tilemap=malloc(GAME_TILEMAP_WIDTH*GAME_TILEMAP_HEIGHT)) == NULL)
            fatal("game_tilemap == NULL");
    }
}

void game_sprite_exit(void) {
    game_normal_setup();

    if (game_tilemap != NULL) {
        free(game_tilemap);
        game_tilemap = NULL;
    }
}

void game_spriteimg_freeimg(struct game_spriteimg *i) {
    free_vrl(&(i->vrl));
}

void game_spriteimg_free(void) {
    unsigned int i;

    for (i=0;i < NUM_SPRITEIMG;i++)
        game_spriteimg_freeimg(game_spriteimg+i);
}

void game_spriteimg_loadimg(struct game_spriteimg *i,const char *path) {
    game_spriteimg_freeimg(i);
    if (load_vrl(&(i->vrl),path) != 0)
        fatal("spriteimg fail %s",path);
}

void game_spriteimg_load(unsigned i,const char *path) {
    game_spriteimg_loadimg(game_spriteimg+i,path);
}

unsigned char game_flags = 0;
unsigned char game_scroll_mode = 0;
unsigned int game_hscroll = 0;
unsigned int game_vscroll = 0;

void game_normal_setup(void) {
    vga_set_stride((vga_state.vga_draw_stride=80u)*4u);
    game_scroll_mode = 0;
    game_hscroll = 0;
    game_vscroll = 0;
}

void game_noscroll_setup(void) {
    vga_set_stride((vga_state.vga_draw_stride=88u)*4u);
    game_scroll_mode = 0;
    game_hscroll = 0;
    game_vscroll = 0;
}

void game_vscroll_setup(void) {
    vga_set_stride((vga_state.vga_draw_stride=88u)*4u);
    game_scroll_mode = GAME_VSCROLL;
    game_hscroll = 0;
    game_vscroll = 0;
}

void game_hscroll_setup(void) {
    vga_set_stride((vga_state.vga_draw_stride=88u)*4u);
    game_scroll_mode = GAME_HSCROLL;
    game_hscroll = 0;
    game_vscroll = 0;
}

void load_tiles(uint16_t ofs,uint16_t ostride,const char *path) {
    struct minipng_reader *rdr;
    unsigned int imgw,imgh;
    unsigned char *row;
    unsigned int i,x;
    uint16_t tof;

    if ((rdr=minipng_reader_open(path)) == NULL)
        fatal("vram_image png error %s",path);
    if (minipng_reader_parse_head(rdr) || rdr->ihdr.width < 4 || rdr->ihdr.width > 32 || (rdr->ihdr.width & 3) != 0 || rdr->ihdr.bit_depth != 8)
        fatal("vram_image png error %s",path);
    if ((row=malloc(imgw)) == NULL)
        fatal("vram_image png error %s",path);

    imgw = rdr->ihdr.width;
    imgh = rdr->ihdr.height;

    if (ostride == 0)
        ostride = imgw/4u;

    for (i=0;i < imgh;i++) {
        minipng_reader_read_idat(rdr,row,1); /* pad byte */
        minipng_reader_read_idat(rdr,row,imgw); /* row */

        vga_write_sequencer(0x02/*map mask*/,0x01);
        for (x=0,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x];

        vga_write_sequencer(0x02/*map mask*/,0x02);
        for (x=1,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x];

        vga_write_sequencer(0x02/*map mask*/,0x04);
        for (x=2,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x];

        vga_write_sequencer(0x02/*map mask*/,0x08);
        for (x=3,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x];

        ofs += ostride;
    }

    minipng_reader_close(&rdr);
    free(row);
}

static inline void game_draw_tile(unsigned o,unsigned i) {
    const unsigned sc = vga_state.vga_draw_stride - 4u;
    const unsigned char far *d = vga_state.vga_graphics_ram+o;
    const unsigned char far *s = MK_FP(0xA000,TILES_VRAM_OFFSET + (i*4u*16u));

    __asm {
        push    ds
        les     di,d
        lds     si,s
        mov     bx,sc
        mov     dx,16
l1:     mov     cx,4
        rep     movsb
        add     di,bx
        dec     dx
        jnz     l1
        pop     ds
    }
}

void game_draw_tiles(unsigned x,unsigned y,unsigned w,unsigned h) {
    unsigned i,ir,o,or,ww,oa;

    w = (w + x + 15u) >> 4u;
    h = (h + y + 15u) >> 4u;
    x >>= 4u;
    y >>= 4u;
    w -= x;
    h -= y;

    /* set up write mode 1 */
    vga_setup_wm1_block_copy();

    /* do it */
    oa = vga_state.vga_draw_stride * 16u;
    or = (y * oa) + (x * 4u);
    ir = (y * GAME_TILEMAP_WIDTH) + x;
    while (h-- > 0) {
        i = ir; o = or; ww = w;
        while (ww-- > 0) {
            game_draw_tile(o,game_tilemap[i++]);
            o += 4u;
        }

        ir += GAME_TILEMAP_WIDTH;
        or += oa;
    }

    /* done */
    vga_restore_rm0wm0();
}

void game_draw_tiles_2pages(unsigned x,unsigned y,unsigned w,unsigned h) {
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_cur_page;
    game_draw_tiles(x,y,w,h);
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    game_draw_tiles(x,y,w,h);
}

void game_draw_sprite(unsigned x,unsigned y,unsigned simg,unsigned flags) {
    /* assume simg < NUM_SPRITEIMG */
    if (game_spriteimg[simg].vrl.vrl_header != NULL) {
        if (flags & GAME_SPRITE_HFLIP) {
            draw_vrl1_vgax_modexclip_hflip(
                    x,y,
                    game_spriteimg[simg].vrl.vrl_header,
                    game_spriteimg[simg].vrl.vrl_lineoffs,
                    game_spriteimg[simg].vrl.buffer+sizeof(*(game_spriteimg[simg].vrl.vrl_header)),
                    game_spriteimg[simg].vrl.bufsz-sizeof(*(game_spriteimg[simg].vrl.vrl_header)));
        }
        else {
            draw_vrl1_vgax_modexclip(
                    x,y,
                    game_spriteimg[simg].vrl.vrl_header,
                    game_spriteimg[simg].vrl.vrl_lineoffs,
                    game_spriteimg[simg].vrl.buffer+sizeof(*(game_spriteimg[simg].vrl.vrl_header)),
                    game_spriteimg[simg].vrl.bufsz-sizeof(*(game_spriteimg[simg].vrl.vrl_header)));
        }
    }
}

void game_update_sprites(void) {
    unsigned int i;

    /* pass 1: draw background beneath all sprites, where the sprite *was* */
    for (i=0;i < game_sprite_max;i++) {
        if (game_sprite[i].ow != 0) {
            game_draw_tiles(game_sprite[i].ox,game_sprite[i].oy,game_sprite[i].ow,game_sprite[i].oh);

            game_sprite[i].oh = game_sprite[i].ph;
            game_sprite[i].ox = game_sprite[i].px;
            game_sprite[i].oy = game_sprite[i].py;

            game_sprite[i].ph = game_sprite[i].h;
            game_sprite[i].px = game_sprite[i].x;
            game_sprite[i].py = game_sprite[i].y;
        }

        game_sprite[i].ow = game_sprite[i].pw;
        game_sprite[i].pw = game_sprite[i].w;
    }

    /* pass 2: draw sprites */
    for (i=0;i < game_sprite_max;i++) {
        if (game_sprite[i].w != 0)
            if (game_sprite[i].flags & GAME_SPRITE_VISIBLE)
                game_draw_sprite(game_sprite[i].x,game_sprite[i].y,game_sprite[i].spriteimg,game_sprite[i].flags);
    }
}

static unsigned char game_0_tilemap[22*14] = {
/*      01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 */
/* 01 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 02 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 03 */ 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
/* 04 */ 7, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7,
/* 05 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 06 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 07 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 08 */ 7, 7, 6, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 6, 7, 7, 7, 7,
/* 09 */ 7, 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 7, 7, 7,
/* 10 */ 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7,
/* 11 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 12 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 13 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
/* 14 */ 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
};

void game_tilecopy(unsigned x,unsigned y,unsigned w,unsigned h,const unsigned char *map) {
    while (h-- > 0) {
        unsigned char *d = game_tilemap + ((y++) * GAME_TILEMAP_WIDTH) + x;
        unsigned int cw = w;

        while (cw-- > 0) *d++ = *map++;
    }
}

/* Smiley Wars */
void game_0() {
    /* sprite slots: smiley 0 (player) and smiley 1 (opponent) */
    const unsigned smiley0=0,smiley1=1;
    /* sprite images */
    const unsigned smi_smile=1,smi_frown=4,smi_bullet=0,smi_win=2,smi_poo=3;
    /* player state (center) */
    unsigned player_x = 90;
    unsigned player_y = 100;
    unsigned player_dir = 0; // 0=right 1=down 2=left 3=up 4=stop 255=not moving 254=frowning
    /* opponent state (center) */
    unsigned opp_x = 230;
    unsigned opp_y = 100;
    unsigned opp_dir = 0; // 0=right 1=down 2=left 3=up 4=stop 255=not moving 254=frowning
    unsigned opp_turn = 1;
    uint32_t opp_turn_next = 0;
    /* smiley size */
    unsigned smilw = 26,smilh = 26,smilox = 32 - 16,smiloy = 32 - 16;
    /* other */
    unsigned int i,amult;
    uint32_t now,pnow;

    if (sin2048fps16_open())
        fatal("cannot open sin2048");

    vga_cur_page = VGA_GAME_PAGE_FIRST;
    vga_set_start_location(vga_cur_page);
    vga_rep_stosw(vga_state.vga_graphics_ram+((320u/4u)*0u),0x0000,((320u/4u)*200u)/2u);
    load_def_pal();

    vga_cur_page = VGA_GAME_PAGE_FIRST;
    vga_next_page = VGA_GAME_PAGE_SECOND;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);

    game_sprite_init();
    game_noscroll_setup();

    load_tiles(TILES_VRAM_OFFSET,0/*default*/,"cr52bt00.png");
    game_spriteimg_load(0,"cr52rn00.vrl");
    game_spriteimg_load(1,"cr52rn01.vrl");
    game_spriteimg_load(2,"cr52rn02.vrl");
    game_spriteimg_load(3,"cr52rn03.vrl");
    game_spriteimg_load(4,"cr52rn04.vrl");

    game_tilecopy(0,0,22,14,game_0_tilemap);

    game_sprite_imgset(smiley0,smi_smile);
    game_sprite_imgset(smiley1,smi_smile);

    game_draw_tiles_2pages(0,0,320,200);

    init_keyboard_irq();
    now = read_timer_counter();

    while (1) {
        pnow = now;
        now = read_timer_counter();

        amult = now - pnow;
        if (amult > 4) amult = 4;

        if (kbdown_test(KBDS_ESCAPE)) break;

        if (kbdown_test(KBDS_LEFT_ARROW)) {
            if (player_dir < 5) {
                player_dir = 2; // 0=right 1=down 2=left 3=up 4=stop
                if (player_x > smilw)
                    player_x -= amult * 2u;
            }
        }
        else if (kbdown_test(KBDS_RIGHT_ARROW)) {
            if (player_dir < 5) {
                player_dir = 0; // 0=right 1=down 2=left 3=up 4=stop
                if (player_x < (320-smilw))
                    player_x += amult * 2u;
            }
        }

        if (kbdown_test(KBDS_UP_ARROW)) {
            if (player_dir < 5) {
                player_dir = 3; // 0=right 1=down 2=left 3=up 4=stop
                if (player_y > smilh)
                    player_y -= amult * 2u;
            }
        }
        else if (kbdown_test(KBDS_DOWN_ARROW)) {
            if (player_dir < 5) {
                player_dir = 1; // 0=right 1=down 2=left 3=up 4=stop
                if (player_y < (200-smilh))
                    player_y += amult * 2u;
            }
        }

        /* opponent: avoid the player if too close */
        if (now < opp_turn_next) {
            // keep going. The purpose of delaying the check again is to prevent
            // cases where the player can pin the opponent in a corner because
            // the opponent is constantly turning due to player proximity.
        }
        else if (abs(opp_x - player_x) < 50 && abs(opp_y - player_y) < 50) {
            if ((opp_x > player_x && opp_dir == 2) || // approaching from the left
                (opp_x < player_x && opp_dir == 0)) { // approaching from the right
                opp_turn_next = now + 30; // 30/120 or 1/4th a second
                opp_turn = (~opp_turn) + 1;
                if (opp_y < ((200*1)/3) || opp_y > ((200*2)/3))
                    opp_dir = (opp_x > player_x) ? 0 : 2; // reverse direction right/left
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
            }
            else if (
                (opp_y > player_y && opp_dir == 3) || // approaching up from below
                (opp_y < player_y && opp_dir == 1)) { // approaching down from above
                opp_turn_next = now + 30; // 30/120 or 1/4th a second
                opp_turn = (~opp_turn) + 1;
                if (opp_x < ((320*1)/3) || opp_x > ((320*2)/3))
                    opp_dir = (opp_y > player_y) ? 1 : 3; // reverse direction up/down
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
            }
        }

        switch (opp_dir) {
            case 0: // moving right
                if (opp_x < (320-smilw))
                    opp_x += amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
            case 1: // moving down
                if (opp_y < (200-smilw))
                    opp_y += amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
            case 2: // moving left
                if (opp_x > smilw)
                    opp_x -= amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
            case 3: // moving up
                if (opp_y > smilh)
                    opp_y -= amult * 2u;
                else
                    opp_dir = (opp_dir + opp_turn) & 3;
                break;
        }

        game_sprite_position(smiley0,player_x - smilox,player_y - smiloy);
        game_sprite_position(smiley1,opp_x    - smilox,opp_y    - smiloy);

        game_update_sprites();
        vga_swap_pages(); /* current <-> next */
        vga_update_disp_cur_page();
        vga_wait_for_vsync(); /* wait for vsync */
    }

    restore_keyboard_irq();
    game_sprite_exit();
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

    memset(game_spriteimg,0,sizeof(game_spriteimg));

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
    if (!init_sndsb()) { // will only fail if lib init fails, will succeed whether or not there is Sound Blaster
        printf("Sound Blaster lib init fail.\n");
        return 1;
    }
	if (init_adlib()) { // may fail if no OPL at 388h
        unsigned int i;

        printf("Adlib FM detected\n");

        memset(adlib_fm,0,sizeof(adlib_fm));
        memset(&adlib_reg_bd,0,sizeof(adlib_reg_bd));
        for (i=0;i < adlib_fm_voices;i++) {
            struct adlib_fm_operator *f;
            f = &adlib_fm[i].mod;
            f->ch_a = f->ch_b = 1; f->ch_c = f->ch_d = 0;
            f = &adlib_fm[i].car;
            f->ch_a = f->ch_b = 1; f->ch_c = f->ch_d = 0;
        }

        for (i=0;i < adlib_fm_voices;i++) {
            struct adlib_fm_operator *f;

            f = &adlib_fm[i].mod;
            f->mod_multiple = 1;
            f->total_level = 63 - 16;
            f->attack_rate = 15;
            f->decay_rate = 0;
            f->sustain_level = 7;
            f->release_rate = 7;
            f->f_number = 0x2AE; // C
            f->octave = 4;
            f->key_on = 0;

            f = &adlib_fm[i].car;
            f->mod_multiple = 1;
            f->total_level = 63 - 16;
            f->attack_rate = 15;
            f->decay_rate = 0;
            f->sustain_level = 7;
            f->release_rate = 7;
            f->f_number = 0;
            f->octave = 0;
            f->key_on = 0;
        }

        adlib_apply_all();
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
            case 0:
                game_0();
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

