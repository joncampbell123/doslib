
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

struct game_spriteimg       game_spriteimg[NUM_SPRITEIMG];

unsigned char               game_sprite_max = 0;
struct game_sprite          game_sprite[NUM_SPRITES];

unsigned char*              game_tilemap = NULL;

unsigned char               game_flags = 0;
unsigned char               game_scroll_mode = 0;
unsigned int                game_hscroll = 0;
unsigned int                game_vscroll = 0;

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
        if (game_sprite[i].ow != 0)
            game_draw_tiles(game_sprite[i].ox,game_sprite[i].oy,game_sprite[i].ow,game_sprite[i].oh);

        game_sprite[i].ow = game_sprite[i].pw;
        game_sprite[i].oh = game_sprite[i].ph;
        game_sprite[i].ox = game_sprite[i].px;
        game_sprite[i].oy = game_sprite[i].py;

        game_sprite[i].pw = game_sprite[i].w;
        game_sprite[i].ph = game_sprite[i].h;
        game_sprite[i].px = game_sprite[i].x;
        game_sprite[i].py = game_sprite[i].y;
    }

    /* pass 2: draw sprites */
    for (i=0;i < game_sprite_max;i++) {
        if (game_sprite[i].w != 0)
            if (game_sprite[i].flags & GAME_SPRITE_VISIBLE)
                game_draw_sprite(game_sprite[i].x,game_sprite[i].y,game_sprite[i].spriteimg,game_sprite[i].flags);
    }
}

