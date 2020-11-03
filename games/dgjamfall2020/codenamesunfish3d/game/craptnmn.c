
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

static void woo_menu_item_coord(unsigned int *x,unsigned int *y,unsigned int i) {
    *x = (40u/4u) + ((140u/4u) * (i/GAMES_PER_COLUMN));
    *y = 40u + ((i%GAMES_PER_COLUMN) * 14u);
}

static void woo_menu_item_draw(unsigned int x,unsigned int y,unsigned int i,unsigned int sel) {
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

