
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "vrlimg.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "fzlibdec.h"
#include "fataexit.h"

/*---------------------------------------------------------------------------*/
/* animation sequence defs                                                   */
/*---------------------------------------------------------------------------*/

struct seq_anim_i {
    unsigned int        duration;       // in ticks
    unsigned int        frame_rate;     // in Hz
    int                 init_frame;
    int                 min_frame;
    int                 max_frame;
    unsigned int        flags;
    signed char         init_dir;
};

#define SEQANF_PINGPONG     (0x01u)
#define SEQANF_OFF          (0x02u)

/*---------------------------------------------------------------------------*/
/* vga, swap current and next page pointers (not yet CRTC)                   */
/*---------------------------------------------------------------------------*/

void vga_swap_pages() {
    vga_cur_page = vga_next_page;
    vga_next_page = (vga_next_page ^ 0x4000) & 0x7FFF;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
}

/*---------------------------------------------------------------------------*/
/* vga, update current page to hardware (reprogram CRTC)                     */
/*---------------------------------------------------------------------------*/

void vga_update_disp_cur_page() {
    /* make sure the CRTC is not in the blanking portions or vsync
     * so that we're not changing offset during a time the CRTC might
     * latch the new display offset.
     *
     * caller is expected to wait for vsync start at some point to
     * keep time with vertical refresh or bad things (flickering)
     * happen. */
    vga_wait_for_vsync_end();
    vga_wait_for_hsync_end();
    vga_set_start_location(vga_cur_page);
}

/*---------------------------------------------------------------------------*/
/* clear next VGA page                                                       */
/*---------------------------------------------------------------------------*/

void vga_clear_npage() {
    vga_write_sequencer(0x02/*map mask*/,0xF);
    vga_rep_stosw(vga_state.vga_graphics_ram,0,0x4000u/2u); /* 16KB (8KB 16-bit WORDS) */
}

struct font_bmp*                    arial_small = NULL;
struct font_bmp*                    arial_medium = NULL;
struct font_bmp*                    arial_large = NULL;

int font_bmp_do_load(struct font_bmp **fnt,const char *path) {
    if (*fnt == NULL) {
        if ((*fnt = font_bmp_load(path)) == NULL)
            return -1;
    }

    return 0;
}

static inline int font_bmp_do_load_arial_large() {
    return font_bmp_do_load(&arial_large,"ariallrg.png");
}

static inline int font_bmp_do_load_arial_medium() {
    return font_bmp_do_load(&arial_medium,"arialmed.png");
}

static inline int font_bmp_do_load_arial_small() {
    return font_bmp_do_load(&arial_small,"arialsml.png");
}

/* yes, we use unicode (UTF-8) strings here in this DOS application! */
int font_bmp_unicode_to_chardef(struct font_bmp *fnt,uint32_t c) {
    if (fnt != NULL && c < 0x10000ul) {
        if (fnt->chardef != NULL) {
            unsigned int i;

            /* NTS: I know, this is very inefficient. Later revisions will add a faster method */
            for (i=0;i < fnt->chardef_count;i++) {
                if (fnt->chardef[i].id == (uint16_t)c)
                    return i;
            }
        }
    }

    return -1;
}

unsigned int font_bmp_draw_chardef(struct font_bmp *fnt,unsigned int cdef,unsigned int x,unsigned int y,unsigned char color) {
    unsigned int nx = x;

    if (fnt != NULL) {
        if (fnt->chardef != NULL && cdef < fnt->chardef_count) {
            const struct font_bmp_chardef *cdent = fnt->chardef + cdef;
            x += (unsigned int)((int)cdent->xoffset);
            y += (unsigned int)((int)cdent->yoffset);
            nx += (unsigned int)((int)cdent->xadvance);
            if (x < 640 && y < 400) {
                const unsigned char *sp = fnt->fontbmp + (fnt->stride * cdent->y) + (cdent->x >> 3u);
                unsigned char *dp = vga_state.vga_graphics_ram + (80u * y) + (x >> 2u);
                unsigned char sm = 0x80u >> (cdent->x & 7u);
                unsigned char dm = 1 << (x & 3u);
                unsigned int sw = cdent->w;

                while (sw-- > 0) {
                    unsigned int sh = cdent->h;
                    const unsigned char *rsp = sp;
                    unsigned char *rdp = dp;

                    vga_write_sequencer(0x02/*map mask*/,dm);
                    while (sh-- > 0) {
                        if ((*rsp) & sm) *rdp = color;
                        rsp += fnt->stride;
                        rdp += 80u;
                    }

                    if ((dm <<= 1u) & 0xF0) {
                        dm = 1u;
                        dp++;
                    }
                    if ((sm >>= 1u) == 0) {
                        sm = 0x80;
                        sp++;
                    }
                }
            }
        }
    }

    return nx;
}

/*---------------------------------------------------------------------------*/
/* introductory sequence                                                     */
/*---------------------------------------------------------------------------*/

#define ANIM_SEQS                   4
#define VRL_IMAGE_FILES             19
#define ATOMPB_PAL_OFFSET           0x00
#define SORC_PAL_OFFSET             0x40
#define PACK_REQ                    0x15

uint32_t sorc_pack_offsets[PACK_REQ+1];
/* PACK contents:

    sorcwoo.pal             // 0 (0x00)
    sorcwoo.sin
    sorcwoo1.vrl
    sorcwoo2.vrl
    sorcwoo3.vrl
    sorcwoo4.vrl            // 5 (0x05)
    sorcwoo5.vrl
    sorcwoo6.vrl
    sorcwoo7.vrl
    sorcwoo8.vrl
    sorcwoo9.vrl            // 10 (0x0A)
    sorcuhhh.vrl
    sorcbwo1.vrl
    sorcbwo2.vrl
    sorcbwo3.vrl
    sorcbwo4.vrl            // 15 (0x0F)
    sorcbwo5.vrl
    sorcbwo6.vrl
    sorcbwo7.vrl
    sorcbwo8.vrl
    sorcbwo9.vrl            // 20 (0x14)
                            //=21 (0x15)
 */

/* VRL files start at index 2 in the pack file */
/* anim1: ping pong loop 0-8 (sorcwoo1.vrl to sorcwoo9.vrl) */
/* anim2: single frame 9 (sorcuhhh.vrl) */
/* anim3: ping pong loop 10-18 (sorcbwo1.vrl to sorcbwo9.vrl) */

/* [0]: sorc "I am the coolest programmer ever woooooooooooo!"
 * [1]: game sprites "Oh great and awesome programmer. What is our purpose in this game?"
 * [2]: sorc "Uhm............"
 * [3]: sorc "I am the coolest programmer ever! I write super awesome code! Wooooooooooooooo!" */

struct seq_anim_i anim_seq[ANIM_SEQS] = {
    /*  dur     fr      if      minf    maxf    fl,                 init_dir */
    {   120*12, 15,     0,      0,      8,      SEQANF_PINGPONG,    1}, // 0
    {   120*12, 1,      0,      0,      0,      SEQANF_OFF,         0}, // 1 [sorc disappears]
    {   120*4,  1,      9,      9,      9,      0,                  0}, // 2
    {   120*14, 30,     10,     10,     18,     SEQANF_PINGPONG,    1}  // 3
};

/* message strings.
 * \n   moves to the next line
 * \x01 clears the text
 * \x02 pauses for 1/4th of a second
 * \x03 pauses for 1 second
 * \x04 fade out text */
const char *anim_text[ANIM_SEQS] = {
    // 0
    "\x01I am the coolest programmer ever!\nI write super awesome optimized code!\x03\x03\x03\x04\x01Wooooooo I'm so cool!\x03\x03\x03\x04",
    // 1
    "\x01Oh great and awesome programmer and\ncreator of our game world.\x03\x03\x03\x04\x01What is our purpose in this game?\x03\x03\x03\x04",
    // 2
    "\x01Uhm\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.\x02.", // no fade out, abrupt change
    // 3
    "\x01I write super optimized clever awesome\nportable code! I am awesome programmer!\x03\x03\x03\x03\x04\x01Wooooooooooooo my code is so cool!\x03\x03\x03\x04"
};

static uint32_t atpb_init_count;

static uint16_t *atpb_sin2048_table = NULL;         // 2048-sample sin(x) quarter wave lookup table (0....0x7FFF)

/* 0x0000 -> sin(0) */
/* 0x0800 -> sin(pi*0.5)    0x0800 = 2048
 * 0x1000 -> sin(pi)        0x1000 = 4096
 * 0x1800 -> sin(pi*1.5)    0x1800 = 6144
 * 0x2000 -> sin(pi*2.0)    0x2000 = 8192 */
int atpb_sin2048_lookup(unsigned int a) {
    int r;

    /* sin(0) to sin(pi) is symmetrical across sin(pi/2) */
    if (a & 0x800u)
        r = atpb_sin2048_table[0x7FFu - (a & 0x7FFu)];
    else
        r = atpb_sin2048_table[a & 0x7FFu];

    /* sin(pi) to sin(pi*2) is just the first half with the opposite sign */
    if (a & 0x1000u)
        r = -r;

    return r;
}

/* cos(x) is just sin(x) shifted by pi/2 (90 degrees) */
inline int atpb_cos2048_lookup(unsigned int a) {
    return atpb_sin2048_lookup(a + 0x800u);
}

/* "Second Reality" style rotozoomer because Woooooooooooo */
void atomic_playboy_zoomer(unsigned int w,unsigned int h,__segment imgseg,uint32_t count) {
    const uint32_t rt = count - atpb_init_count;
    const __segment vseg = FP_SEG(vga_state.vga_graphics_ram);

    // scale, to zoom in and out
    const long scale = ((long)atpb_sin2048_lookup(rt * 5ul) >> 1l) + 0xA000l;
    // column-step. multiplied 4x because we're rendering only every 4 pixels for smoothness.
    const uint16_t sx1 = (uint16_t)(((((long)atpb_cos2048_lookup(rt * 10ul) *  0x400l) >> 15l) * scale) >> 15l);
    const uint16_t sy1 = (uint16_t)(((((long)atpb_sin2048_lookup(rt * 10ul) * -0x400l) >> 15l) * scale) >> 15l);
    // row-step. multiplied by 1.2 (240/200) to compensate for 320x200 non-square pixels. (1.2 * 0x100) = 0x133
    const uint16_t sx2 = (uint16_t)(((((long)atpb_cos2048_lookup(rt * 10ul) *  0x133l) >> 15l) * scale) >> 15l);
    const uint16_t sy2 = (uint16_t)(((((long)atpb_sin2048_lookup(rt * 10ul) * -0x133l) >> 15l) * scale) >> 15l);

    unsigned cvo = FP_OFF(vga_state.vga_graphics_ram);
    uint16_t fcx,fcy;

// make sure rotozoomer is centered on screen
    fcx = 0 - ((w / 2u / 4u) * sx1) - ((h / 2u) *  sy2);
    fcy = 0 - ((w / 2u / 4u) * sy1) - ((h / 2u) * -sx2);

// NTS: Because of VGA unchained mode X, this renders the effect in vertical columns rather than horizontal rows
    while (w >= 4) {
        vga_write_sequencer(0x02/*map mask*/,0x0F);

        // WARNING: This loop temporarily modifies DS. While DS is altered do not access anything by name in the data segment.
        //          However variables declared locally in this function are OK because they are allocated from the stack, and
        //          are referenced using [bp] which uses the stack (SS) register.
        __asm {
            mov     cx,h
            mov     es,vseg
            mov     di,cvo
            inc     cvo
            push    ds
            mov     ds,imgseg
            mov     dx,fcx          ; DX = fractional X coord
            mov     si,fcy          ; SI = fractional Y coord

crloop:
            ; BX = (SI & 0xFF00) + (DX >> 8)
            ;   but DX/BX are the general registers with hi/lo access so
            ; BX = (SI & 0xFF00) + DH
            ; by the way on anything above a 386 that sort of optimization stuff doesn't help performance.
            ; later (Pentium Pro) processors don't like it when you alternate between DH/DL and DX because
            ; of the way the processor works internally regarding out of order execution and register renaming.
            mov     bx,si
            mov     bl,dh
            mov     al,[bx]
            stosb
            add     di,79           ; stosb / add di,79  is equivalent to  mov es:[di],al / add di,80

            add     dx,sy2
            sub     si,sx2

            loop    crloop
            pop     ds
        }

        fcx += sx1;
        fcy += sy1;
        w -= 4;
    }
}

static const int animtext_init_x = 10;
static const int animtext_init_y = 168;
static const unsigned char animtext_color_init = 255;

/* atomic playboy background 256x256 */
int rzbkload(unsigned atpbseg,const char *path) {
    struct minipng_reader *rdr;

    /* WARNING: Code assumes 16-bit large memory model! */

    if ((rdr=minipng_reader_open(path)) == NULL)
        return -1;

    if (minipng_reader_parse_head(rdr) || rdr->plte == NULL || rdr->plte_count == 0) {
        minipng_reader_close(&rdr);
        return -1;
    }

    {
        unsigned int i;

        for (i=0;i < 256;i++) {
            unsigned char far *imgptr = (unsigned char far*)MK_FP(atpbseg + (i * 16u/*paragraphs=256b*/),0);
            minipng_reader_read_idat(rdr,imgptr,1); /* pad byte */
            minipng_reader_read_idat(rdr,imgptr,256); /* row */
        }

        {
            const unsigned char *p = (const unsigned char*)(rdr->plte);
            vga_palette_lseek(ATOMPB_PAL_OFFSET);
            for (i=0;i < rdr->plte_count;i++)
                vga_palette_write(p[(i*3)+0]>>2,p[(i*3)+1]>>2,p[(i*3)+2]>>2);
        }
    }

    minipng_reader_close(&rdr);
    return 0;
}

void seq_intro() {
    unsigned char animtext_bright = 63;
    unsigned char animtext_color = animtext_color_init;
    const unsigned char at_interval = 120u / 20ul; /* 20 chars/second */
    int animtext_lineheight = 14;
    int animtext_x = animtext_init_x;
    int animtext_y = animtext_init_y;
    struct font_bmp *animtext_fnt = NULL;
    const char *animtext = "";
    uint32_t nanim_count = 0;
    uint16_t vrl_anim_interval = 0;
    uint32_t nf_count = 0,ccount = 0,atcount = 0;
    struct vrl_image vrl_image[VRL_IMAGE_FILES];
    signed char vrl_image_dir = 0;
    int vrl_image_select = 0;
    unsigned atpbseg; /* atomic playboy 256x256 background DOS segment value */
    unsigned char anim;
    int redraw = 1;
    int packfd;
    int c;

    /* need arial medium */
    if (font_bmp_do_load_arial_medium())
        fatal("cannot load arial font");

    animtext_fnt = arial_medium;

    /* our assets are in a pack now */
    if ((packfd=open("sorcwoo.vrp",O_RDONLY|O_BINARY)) < 0)
        fatal("cannot open sorcwoo pack");

    {
        uint16_t count;
        if (read(packfd,&count,2) != 2 || count < PACK_REQ)
            fatal("cannot open sorcwoo pack");
    }

    if (read(packfd,sorc_pack_offsets,(PACK_REQ+1)*4) != ((PACK_REQ+1)*4))
        fatal("cannot open sorcwoo pack");

    /* the rotozoomer effect needs a sin lookup table */
    atpb_sin2048_table = malloc(sizeof(uint16_t) * 2048);
    if (atpb_sin2048_table == NULL) fatal("sorcwoo.sin");
    lseek(packfd,sorc_pack_offsets[1],SEEK_SET);
    read(packfd,atpb_sin2048_table,sizeof(uint16_t) * 2048);

    /* rotozoomer background */
    if (_dos_allocmem(0x1000/*paragrahs==64KB*/,&atpbseg) != 0)
        fatal("seq_intro: failed atmpbrz.png");

    /* text color */
    vga_palette_lseek(0xFF);
    vga_palette_write(63,63,63);

    /* sorc palette */
    lseek(packfd,sorc_pack_offsets[0],SEEK_SET);
    read(packfd,common_tmp_small,32*3);
    pal_buf_to_vga(/*offset*/SORC_PAL_OFFSET,/*count*/32,common_tmp_small);

    {
        unsigned int vrl_image_count = 0;
        for (vrl_image_count=0;vrl_image_count < VRL_IMAGE_FILES;vrl_image_count++) {
            lseek(packfd,sorc_pack_offsets[2+vrl_image_count],SEEK_SET);
            if (load_vrl_fd(&vrl_image[vrl_image_count],packfd,sorc_pack_offsets[2+1+vrl_image_count] - sorc_pack_offsets[2+vrl_image_count]) != 0)
                fatal("seq_intro: unable to load VRL %u at %lu",vrl_image_count,sorc_pack_offsets[2+vrl_image_count]);

                vrl_palrebase(
                    vrl_image[vrl_image_count].vrl_header,
                    vrl_image[vrl_image_count].vrl_lineoffs,
                    vrl_image[vrl_image_count].buffer+sizeof(*(vrl_image[vrl_image_count].vrl_header)),
                    SORC_PAL_OFFSET);
        }
    }

    atpb_init_count = atcount = nanim_count = ccount = read_timer_counter();
    anim = -1; /* increment to zero in loop */

    vga_clear_npage();

    do {
        if (ccount >= nanim_count) {
            if ((++anim) >= ANIM_SEQS) break;

            if (anim == 0) {
                if (rzbkload(atpbseg,"wxpbrz.png"))
                    fatal("wxpbrz.png");

                nanim_count = ccount = read_timer_counter();
            }
            else if (anim == 2) { /* use the idle downtime of the "uhhhhhhhh" to load it */
                if (rzbkload(atpbseg,"atmpbrz.png"))
                    fatal("atmpbrz.png");

                nanim_count = ccount = read_timer_counter();
            }

            vrl_anim_interval = (uint16_t)(timer_tick_rate / anim_seq[anim].frame_rate);
            vrl_image_select = anim_seq[anim].init_frame;
            vrl_image_dir = anim_seq[anim].init_dir;

            nf_count = nanim_count + vrl_anim_interval;
            atcount = nanim_count + at_interval;

            animtext = anim_text[anim];

            nanim_count += anim_seq[anim].duration;
            if (nanim_count < ccount) nanim_count = ccount;
            redraw = 1;
        }
        else if (anim == 0 || anim == 3) {
            redraw = 1; /* always redraw for that super awesome rotozoomer effect :) */
        }

        if (redraw) {
            redraw = 0;

            if (anim == 0 || anim == 3)
                atomic_playboy_zoomer(320/*width*/,168/*height*/,atpbseg,ccount);
            else {
                vga_write_sequencer(0x02/*map mask*/,0xF);
                vga_rep_stosw(vga_state.vga_graphics_ram,0,((320u/4u)*168u)/2u);
            }

            if (!(anim_seq[anim].flags & SEQANF_OFF)) {
                draw_vrl1_vgax_modex(70,0,
                    vrl_image[vrl_image_select].vrl_header,
                    vrl_image[vrl_image_select].vrl_lineoffs,
                    vrl_image[vrl_image_select].buffer+sizeof(*vrl_image[vrl_image_select].vrl_header),
                    vrl_image[vrl_image_select].bufsz-sizeof(*vrl_image[vrl_image_select].vrl_header));
            }

            vga_swap_pages(); /* current <-> next */
            vga_update_disp_cur_page();
            vga_wait_for_vsync(); /* wait for vsync */
        }

        while (ccount >= atcount) {
            uint32_t last_atcount = atcount;

            atcount += at_interval;

            if (*animtext != 0) {
                switch (*animtext) {
                    case 0x01: // clear text
                        animtext_x = animtext_init_x;
                        animtext_y = animtext_init_y;
                        vga_write_sequencer(0x02/*map mask*/,0x0F);
                        vga_rep_stosw((unsigned char far*)MK_FP(0xA000,((320u/4u)*168u)+VGA_PAGE_FIRST),0,((320u/4u)*(200u-168u))/2u);
                        vga_rep_stosw((unsigned char far*)MK_FP(0xA000,((320u/4u)*168u)+VGA_PAGE_SECOND),0,((320u/4u)*(200u-168u))/2u);

                        animtext_bright = 63;
                        vga_palette_lseek(0xFF);
                        vga_palette_write(animtext_bright,animtext_bright,animtext_bright);

                        animtext++;
                        break;
                    case 0x02: // 1/4-sec pause
                        atcount = last_atcount + (120ul / 4ul);
                        animtext++;
                        break;
                    case 0x03: // 1-sec pause
                        atcount = last_atcount + 120ul;
                        animtext++;
                        break;
                    case 0x04: // fade out
                        if (animtext_bright > 0) {
                            atcount = last_atcount + (120ul / 60ul);
                            if (animtext_bright >= 4)
                                animtext_bright -= 4;
                            else
                                animtext_bright = 0;

                            vga_palette_lseek(0xFF);
                            vga_palette_write(animtext_bright,animtext_bright,animtext_bright);
                        }
                        else {
                            animtext++; /* do not advance until fade out */
                        }
                        break;
                    case '\n': // newline
                        animtext_x = animtext_init_x;
                        animtext_y += animtext_lineheight;//FIXME: Font should specify height
                        animtext++;
                        break;
                    default: // print text
                        {
                            const uint32_t c = utf8decode(&animtext);
                            if (c != 0ul) {
                                unsigned char far *sp = vga_state.vga_graphics_ram;
                                const uint32_t cdef = font_bmp_unicode_to_chardef(animtext_fnt,c);

                                vga_state.vga_graphics_ram = (unsigned char far*)MK_FP(0xA000,VGA_PAGE_FIRST);
                                font_bmp_draw_chardef(animtext_fnt,cdef,animtext_x,animtext_y,animtext_color);

                                vga_state.vga_graphics_ram = (unsigned char far*)MK_FP(0xA000,VGA_PAGE_SECOND);
                                animtext_x = font_bmp_draw_chardef(animtext_fnt,cdef,animtext_x,animtext_y,animtext_color);

                                vga_state.vga_graphics_ram = sp;
                            }
                        }
                        break;
                }
            }
        }

        if (kbhit()) {
            c = getch();
            if (c == 27) break;
        }

        while (ccount >= nf_count) {
            if (!(anim_seq[anim].flags & SEQANF_OFF)) {
                redraw = 1;
                if (vrl_image_select >= anim_seq[anim].max_frame) {
                    if (anim_seq[anim].flags & SEQANF_PINGPONG) {
                        vrl_image_select = anim_seq[anim].max_frame - 1;
                        vrl_image_dir = -1;
                    }
                }
                else if (vrl_image_select <= anim_seq[anim].min_frame) {
                    if (anim_seq[anim].flags & SEQANF_PINGPONG) {
                        vrl_image_select = anim_seq[anim].min_frame + 1;
                        vrl_image_dir = 1;
                    }
                }
                else {
                    vrl_image_select += (int)vrl_image_dir; /* sign extend */
                }
            }

            nf_count += vrl_anim_interval;
        }

        ccount = read_timer_counter();
    } while (1);

    /* sinus table */
    free(atpb_sin2048_table);
    atpb_sin2048_table = NULL;
    /* atomic playboy image seg */
    _dos_freemem(atpbseg);
    /* VRLs */
    for (vrl_image_select=0;vrl_image_select < VRL_IMAGE_FILES;vrl_image_select++)
        free_vrl(&vrl_image[vrl_image_select]);

    close(packfd);
#undef ATOMPB_PAL_OFFSET
#undef SORC_PAL_OFFSET
#undef VRL_IMAGE_FILES
#undef ANIM_SEQS
#undef PACK_REQ
}

/*---------------------------------------------------------------------------*/
/* main                                                                      */
/*---------------------------------------------------------------------------*/

int main() {
    probe_dos();
	cpu_probe();
    if (cpu_basic_level < 3) {
        printf("This game requires a 386 or higher\n");
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

#if TARGET_MSDOS == 16
# if 0 // not using it yet
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support
# endif
#endif

    init_timer_irq();
    init_vga256unchained();

    seq_intro();

    font_bmp_free(&arial_small);
    font_bmp_free(&arial_medium);
    font_bmp_free(&arial_large);

    unhook_irqs();
    restore_text_mode();

    return 0;
}

