
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
#include "fonts.h"
#include "vrlimg.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"

#if 0//OLD CODE
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
/* introductory sequence                                                     */
/*---------------------------------------------------------------------------*/

#define ANIM_SEQS                   4
#define VRL_IMAGE_FILES             19
#define ATOMPB_PAL_OFFSET           0x00
#define SORC_PAL_OFFSET             0x40
#define PACK_REQ                    0x15

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

static const int animtext_init_x = 10;
static const int animtext_init_y = 168;
static const unsigned char animtext_color_init = 255;

void seq_intro() {
    uint32_t rotozoomer_init_count;
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
    unsigned rotozoomerimgseg; /* atomic playboy 256x256 background DOS segment value */
    unsigned char anim;
    int redraw = 1;
    int c;

    /* need arial medium */
    if (font_bmp_do_load_arial_medium())
        fatal("cannot load arial font");
    if (sorc_pack_open())
        fatal("cannot open sorcwoo pack");
    if (sorc_pack->offset_count < PACK_REQ)
        fatal("cannot open sorcwoo pack");
    if (sin2048fps16_open())
        fatal("cannot open sin2048");
    if ((rotozoomerimgseg=rotozoomer_imgalloc()) == 0)
        fatal("rotozoomer bkgnd");

    animtext_fnt = arial_medium;

    /* text color */
    vga_palette_lseek(0xFF);
    vga_palette_write(63,63,63);

    /* sorc palette */
    lseek(sorc_pack->fd,dumbpack_ent_offset(sorc_pack,0),SEEK_SET);
    read(sorc_pack->fd,common_tmp_small,32*3);
    pal_buf_to_vga(/*offset*/SORC_PAL_OFFSET,/*count*/32,common_tmp_small);

    {
        unsigned int vrl_image_count = 0;
        for (vrl_image_count=0;vrl_image_count < VRL_IMAGE_FILES;vrl_image_count++) {
            lseek(sorc_pack->fd,dumbpack_ent_offset(sorc_pack,2+vrl_image_count),SEEK_SET);
            if (load_vrl_fd(&vrl_image[vrl_image_count],sorc_pack->fd,dumbpack_ent_size(sorc_pack,2+vrl_image_count)) != 0)
                fatal("seq_intro: unable to load VRL %u",vrl_image_count);

                vrl_palrebase(
                    vrl_image[vrl_image_count].vrl_header,
                    vrl_image[vrl_image_count].vrl_lineoffs,
                    vrl_image[vrl_image_count].buffer+sizeof(*(vrl_image[vrl_image_count].vrl_header)),
                    SORC_PAL_OFFSET);
        }
    }

    rotozoomer_init_count = atcount = nanim_count = ccount = read_timer_counter();
    anim = -1; /* increment to zero in loop */

    vga_clear_npage();

    do {
        if (ccount >= nanim_count) {
            if ((++anim) >= ANIM_SEQS) break;

            if (anim == 0) {
                if (rotozoomerpngload(rotozoomerimgseg,"wxpbrz.png",ATOMPB_PAL_OFFSET))
                    fatal("wxpbrz.png");

                nanim_count = ccount = read_timer_counter();
            }
            else if (anim == 2) { /* use the idle downtime of the "uhhhhhhhh" to load it */
                if (rotozoomerpngload(rotozoomerimgseg,"atmpbrz.png",ATOMPB_PAL_OFFSET))
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
                rotozoomer_fast_effect(320/*width*/,168/*height*/,rotozoomerimgseg,ccount - rotozoomer_init_count);
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
                                font_bmp_draw_chardef_vga8u(animtext_fnt,cdef,animtext_x,animtext_y,animtext_color);

                                vga_state.vga_graphics_ram = (unsigned char far*)MK_FP(0xA000,VGA_PAGE_SECOND);
                                animtext_x = font_bmp_draw_chardef_vga8u(animtext_fnt,cdef,animtext_x,animtext_y,animtext_color);

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

    /* VRLs */
    rotozoomer_imgfree(&rotozoomerimgseg);
    for (vrl_image_select=0;vrl_image_select < VRL_IMAGE_FILES;vrl_image_select++)
        free_vrl(&vrl_image[vrl_image_select]);
#undef ATOMPB_PAL_OFFSET
#undef SORC_PAL_OFFSET
#undef VRL_IMAGE_FILES
#undef ANIM_SEQS
#undef PACK_REQ
}
#endif

/* sequence animation engine (for VGA page flipping animation) */
enum seqcanvas_layer_what {
    SEQCL_NONE=0,                                       /* nothing */
    SEQCL_MSETFILL,                                     /* msetfill */
    SEQCL_ROTOZOOM,                                     /* rotozoom */
    SEQCL_VRL,                                          /* vrl */
    SEQCL_CALLBACK,                                     /* callback */
    SEQCL_TEXT,                                         /* text */

    SEQCL_MAX
};

union seqcanvas_layeru_t;

struct seqcanvas_memsetfill {
    unsigned int                    h;                  /* where to fill */
    unsigned char                   c;                  /* with what */
};

struct seqcanvas_rotozoom {
    unsigned                        imgseg;             /* segment containing 256x256 image to rotozoom */
    uint32_t                        time_base;          /* counter time base to rotozoom by or (~0ul) if not to automatically rotozoom */
    unsigned int                    h;                  /* how many scanlines */
};

struct seqcanvas_vrl {
    struct vrl_image*               vrl;                /* vrl image to draw */
    unsigned int                    x,y;                /* where to draw */
};

struct seqcanvas_callback {
    void                            (*fn)(union seqcanvas_layeru_t *layer);
    uint32_t                        param1,param2;
};

struct seqcanvas_text {
    unsigned int                    textcdef_length;
    uint16_t*                       textcdef;           /* text to draw (as chardef indexes) */
    struct font_bmp*                font;               /* font to use */
    int                             x,y;                /* where to start drawing */
};

union seqcanvas_layeru_t {
    struct seqcanvas_memsetfill     msetfill;           /* memset (solid color) */
    struct seqcanvas_rotozoom       rotozoom;           /* draw a rotozoomer */
    struct seqcanvas_vrl            vrl;                /* draw VRL image */
    struct seqcanvas_callback       callback;           /* callback function */
    struct seqcanvas_text           text;               /* draw text */
};

struct seqcanvas_layer_t {
    unsigned char                   what;
    union seqcanvas_layeru_t        rop;
};

#define SEQAF_REDRAW                (1u << 0u)          /* redraw the canvas and page flip to screen */
#define SEQAF_END                   (1u << 1u)
#define SEQAF_TEXT_PALCOLOR_UPDATE  (1u << 2u)

/* param1 of SEQAEV_TEXT_CLEAR */
#define SEQAEV_TEXT_CLEAR_FLAG_NOPALUPDATE (1u << 0u)

/* param1 of SEQAEV_TEXT */
#define SEQAEV_TEXT_FLAG_NOWAIT         (1u << 0u)

enum {
    SEQAEV_END=0,                   /* end of sequence */
    SEQAEV_TEXT_CLEAR,              /* clear/reset/home text */
    SEQAEV_TEXT,                    /* print text (UTF-8 string pointed to by 'params') with control codes embedded as well */
    SEQAEV_TEXT_FADEOUT,            /* fade out (palette entry for) text. param1 is how much to subtract from R/G/B. At 120Hz a value of 255 is just over 2 seconds. 0 means use default. */
    SEQAEV_TEXT_FADEIN,             /* fade in to RGB 888 in param2, or if param2 == 0 to default palette color. param1 same as FADEOUT */
    SEQAEV_WAIT,                    /* pause for 'param1' tick counts */
    SEQAEV_SYNC,                    /* set next event time to now */
    SEQAEV_SEQUENCE,                /* sequence event (function calling animator catches this value and does something) */

    SEQAEV_MAX
};

struct seqanim_event_t {
    unsigned char                   what;
    uint32_t                        param1,param2;
    const char*                     params;
};

struct seqanim_text_t {
    int                             home_x,home_y;      /* home position when clearing text. also used on newline. */
    int                             end_x,end_y;        /* right margin, bottom margin */
    int                             x,y;                /* current position */
    unsigned char                   color;              /* current color */
    struct font_bmp*                font;               /* current font */
    uint32_t                        delay;              /* printout delay */
    struct font_bmp*                def_font;           /* default font */
    uint32_t                        def_delay;          /* printout delay */
    uint8_t                         palcolor[3];        /* VGA palette color */
    uint8_t                         def_palcolor[3];    /* default VGA palette color */
    const char*                     msg;                /* UTF-8 string to print */
};

struct seqanim_t {
    /* what to draw (back to front) */
    unsigned int                    canvas_obj_alloc;   /* how much is allocated */
    unsigned int                    canvas_obj_count;   /* how much to draw */
    struct seqcanvas_layer_t*       canvas_obj;
    /* when to process next event */
    uint32_t                        next_event;         /* counter value */
    /* events to process, provided by caller (not allocated) */
    const struct seqanim_event_t*   events;
    /* text print (dialogue) state */
    struct seqanim_text_t           text;
    /* state flags */
    unsigned int                    flags;
};

void seqcanvas_text_free_text(struct seqcanvas_text *t) {
    if (t->textcdef) {
        free(t->textcdef);
        t->textcdef = NULL;
    }
}

int seqcanvas_text_alloc_text(struct seqcanvas_text *t,unsigned int len) {
    seqcanvas_text_free_text(t);
    if (len != 0 && len < 1024) {
        t->textcdef_length = len;
        if ((t->textcdef=malloc(sizeof(uint16_t) * len)) == NULL)
            return -1;
    }

    return 0;
}

void seqcanvas_clear_layer(struct seqcanvas_layer_t *l) {
    switch (l->what) {
        case SEQCL_TEXT:
            seqcanvas_text_free_text(&(l->rop.text));
            break;
    }

    l->what = SEQCL_NONE;    
}

int seqanim_alloc_canvas(struct seqanim_t *sa,unsigned int max) {
    if (sa->canvas_obj == NULL) {
        if (max > 64) return -1;
        sa->canvas_obj_alloc = max;
        sa->canvas_obj_count = 0;
        if ((sa->canvas_obj=calloc(max,sizeof(struct seqcanvas_layer_t))) == NULL) return -1; /* calloc will zero fill */
    }

    return 0;
}

void seqanim_free_canvas(struct seqanim_t *sa) {
    unsigned int i;

    if (sa->canvas_obj) {
        for (i=0;i < sa->canvas_obj_alloc;i++) seqcanvas_clear_layer(&(sa->canvas_obj[i]));   
        free(sa->canvas_obj);
        sa->canvas_obj=NULL;
    }
}

struct seqanim_t *seqanim_alloc(void) {
    struct seqanim_t *sa = calloc(1,sizeof(struct seqanim_t));

    if (sa != NULL) {
        sa->text.color = 0xFF;
        sa->text.font = sa->text.def_font = arial_medium; /* hope you loaded this before calling this alloc or you will have to assign this yourself! */
        sa->text.delay = sa->text.def_delay = 120 / 20;
        sa->text.palcolor[0] = sa->text.def_palcolor[0] = 255;
        sa->text.palcolor[1] = sa->text.def_palcolor[1] = 255;
        sa->text.palcolor[2] = sa->text.def_palcolor[2] = 255;
    }

    return sa;
}

void seqanim_free(struct seqanim_t **sa) {
    if (*sa != NULL) {
        seqanim_free_canvas(*sa);
        free(*sa);
        *sa = NULL;
    }
}

static inline int seqanim_running(struct seqanim_t *sa) {
    if (sa->flags & SEQAF_END)
        return 0;

    return 1;
}

static inline void seqanim_set_redraw_everything_flag(struct seqanim_t *sa) {
    sa->flags |= SEQAF_REDRAW | SEQAF_TEXT_PALCOLOR_UPDATE;
}

void seqanim_text_clear(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    (void)e; // unused

    sa->text.delay = sa->text.def_delay;
    sa->text.font = sa->text.def_font;
    sa->text.x = sa->text.home_x;
    sa->text.y = sa->text.home_y;
    sa->text.color = 0xFF;

    if (!(e->param1 & SEQAEV_TEXT_CLEAR_FLAG_NOPALUPDATE)) {
        sa->text.palcolor[0] = sa->text.def_palcolor[0];
        sa->text.palcolor[1] = sa->text.def_palcolor[1];
        sa->text.palcolor[2] = sa->text.def_palcolor[2];
        sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;
    }

    if (sa->text.home_y < sa->text.end_y) {
        vga_write_sequencer(0x02/*map mask*/,0xF);
        vga_rep_stosw(orig_vga_graphics_ram + VGA_PAGE_FIRST + (sa->text.home_y*80u),0,((320u/4u)*(sa->text.end_y - sa->text.home_y))/2u);
        vga_rep_stosw(orig_vga_graphics_ram + VGA_PAGE_SECOND + (sa->text.home_y*80u),0,((320u/4u)*(sa->text.end_y - sa->text.home_y))/2u);
    }
}

unsigned int seqanim_text_height(struct seqanim_t *sa) {
    // FIXME: font_bmp needs to define line height!
    if (sa->text.font == arial_medium)
        return 14;

    return 0;
}

void seqanim_step_text(struct seqanim_t *sa,const uint32_t nowcount,const struct seqanim_event_t *e) {
    uint32_t c;

    (void)nowcount; // unused
    (void)e; // unused

    c = utf8decode(&(sa->text.msg));
    if (c != 0) {
        switch (c) {
            case 0x01:
                sa->next_event += 120u / 4u;
                break;
            case 0x02:
                sa->next_event += 120u;
                break;
            case 0x10:
                if (*(sa->text.msg) == 0) break; /* even as a two-byte sequence please don't allow 0x00 to avoid confusion with end of string */
                sa->text.delay = *(sa->text.msg++) - 1u;
                break;
            case '\n': {
                const unsigned int lh = seqanim_text_height(sa);
                sa->text.x = sa->text.home_x;
                sa->text.y += lh;
                } break;
            default: {
                unsigned char far *sp = vga_state.vga_graphics_ram;
                const uint32_t cdef = font_bmp_unicode_to_chardef(sa->text.font,c);

                vga_state.vga_graphics_ram = orig_vga_graphics_ram + VGA_PAGE_FIRST;
                font_bmp_draw_chardef_vga8u(sa->text.font,cdef,sa->text.x,sa->text.y,sa->text.color);

                vga_state.vga_graphics_ram = orig_vga_graphics_ram + VGA_PAGE_SECOND;
                sa->text.x = font_bmp_draw_chardef_vga8u(sa->text.font,cdef,sa->text.x,sa->text.y,sa->text.color);

                vga_state.vga_graphics_ram = sp;

                if (!(e->param1 & SEQAEV_TEXT_FLAG_NOWAIT))
                    sa->next_event += sa->text.delay;
                } break;
        }
    }
    else {
        (sa->events)++; /* next */
    }
}

void seqanim_step_text_fadein(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    const unsigned char sub = (e->param1 != 0) ? e->param1 : 8;
    uint8_t color[3];
    unsigned int i;

    if (e->param2 != 0) {
        color[0] = (unsigned char)(e->param2 >> 16u);
        color[1] = (unsigned char)(e->param2 >>  8u);
        color[2] = (unsigned char)(e->param2 >>  0u);
    }
    else {
        color[0] = sa->text.def_palcolor[0];
        color[1] = sa->text.def_palcolor[1];
        color[2] = sa->text.def_palcolor[2];
    }

    for (i=0;i < 3;i++) {
        const unsigned int s = (unsigned int)sa->text.palcolor[i] + (unsigned int)sub;

        if (s < (unsigned int)color[i])
            sa->text.palcolor[i] = (unsigned char)s;
        else
            sa->text.palcolor[i] = color[i];
    }

    if (sa->text.palcolor[0] == color[0] && sa->text.palcolor[1] == color[1] && sa->text.palcolor[2] == color[2])
        (sa->events)++; /* next */
    else
        sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;
}

void seqanim_step_text_fadeout(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    const unsigned char sub = (e->param1 != 0) ? e->param1 : 8;
    unsigned int i;

    for (i=0;i < 3;i++) {
        if (sa->text.palcolor[i] >= sub)
            sa->text.palcolor[i] -= sub;
        else
            sa->text.palcolor[i] = 0;
    }

    if ((sa->text.palcolor[0] | sa->text.palcolor[1] | sa->text.palcolor[2]) == 0)
        (sa->events)++; /* next */
    else
        sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;
}

void seqanim_step(struct seqanim_t *sa,const uint32_t nowcount) {
    if (nowcount >= sa->next_event) {
        if (sa->events == NULL) {
            sa->flags |= SEQAF_END;
        }
        else {
            const struct seqanim_event_t *e = sa->events;

            switch (sa->events->what) {
                case SEQAEV_END:
                    sa->next_event = nowcount;
                    sa->flags |= SEQAF_END;
                    break;
                case SEQAEV_TEXT_CLEAR:
                    sa->next_event = nowcount;
                    seqanim_text_clear(sa,e);
                    (sa->events)++; /* next */
                    break;
                case SEQAEV_TEXT:
                    seqanim_step_text(sa,nowcount,e); /* will advance sa->events */
                    break;
                case SEQAEV_TEXT_FADEOUT:
                    seqanim_step_text_fadeout(sa,e);
                    break;
                case SEQAEV_TEXT_FADEIN:
                    seqanim_step_text_fadein(sa,e);
                    break;
                case SEQAEV_WAIT:
                    sa->next_event += e->param1;
                    (sa->events)++; /* next */
                    break;
                case SEQAEV_SYNC:
                    sa->next_event = nowcount;
                    (sa->events)++; /* next */
                    break;
                default:
                    (sa->events)++; /* next */
                    break;
            }

            if (sa->events != e) {
                /* some init required for next event */
                switch (sa->events->what) {
                    case SEQAEV_TEXT:
                        sa->text.msg = sa->events->params;
                        break;
                }
            }
        }
    }
}

void seqanim_draw(struct seqanim_t *sa) {
    sa->flags &= ~SEQAF_REDRAW;
}

void seqanim_update_text_palcolor(struct seqanim_t *sa) {
    sa->flags &= ~SEQAF_TEXT_PALCOLOR_UPDATE;

    vga_palette_lseek(sa->text.color);
    vga_palette_write(sa->text.palcolor[0]>>2u,sa->text.palcolor[1]>>2u,sa->text.palcolor[2]>>2u);
}

void seqanim_redraw(struct seqanim_t *sa) {
    const unsigned int oflags = sa->flags;

    if (sa->flags & SEQAF_REDRAW) {
        seqanim_draw(sa);

        vga_swap_pages(); /* current <-> next */
        vga_update_disp_cur_page();
    }
    if (sa->flags & SEQAF_TEXT_PALCOLOR_UPDATE) {
        seqanim_update_text_palcolor(sa);
    }

    if (oflags & (SEQAF_REDRAW|SEQAF_TEXT_PALCOLOR_UPDATE)) {
        vga_wait_for_vsync(); /* wait for vsync */
    }
}

/*---------------------------------------------------------------------------*/
/* introduction sequence                                                     */
/*---------------------------------------------------------------------------*/

const struct seqanim_event_t seq_intro_events[] = {
//  what                    param1,     param2,     params
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Hello world!\n" "\x10\x3C"/*slow down*/ "How are you?"},
    {SEQAEV_WAIT,           120*5,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Hey, blah blah blah blah blah! \x02" "\x10\x01"/*instant*/ "Uhm\x01.\x01.\x01.\x01.\x01.\x01.\x01.\x01.\x01.\x01."},
    {SEQAEV_WAIT,           120*1,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     SEQAEV_TEXT_CLEAR_FLAG_NOPALUPDATE},
    {SEQAEV_TEXT,           SEQAEV_TEXT_FLAG_NOWAIT,0,"Instant text display!\nWoo yeah!"},
    {SEQAEV_TEXT_FADEIN,    0,          0,          NULL},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_END}
};

void seq_intro(void) {
#define ANIM_HEIGHT         168
#define ANIM_TEXT_TOP       168
#define ANIM_TEXT_LEFT      5
#define ANIM_TEXT_RIGHT     310
#define ANIM_TEXT_BOTTOM    198
    struct seqanim_t *sanim;
    uint32_t nowcount;
    int c;

    /* if we load this now, seqanim can automatically use it */
    if (font_bmp_do_load_arial_medium())
        fatal("arial");

    if ((sanim=seqanim_alloc()) == NULL)
        fatal("seqanim");
    if (seqanim_alloc_canvas(sanim,32))
        fatal("seqanim");

    sanim->next_event = read_timer_counter();

    sanim->events = seq_intro_events;

    sanim->text.home_x = sanim->text.x = ANIM_TEXT_LEFT;
    sanim->text.home_y = sanim->text.y = ANIM_TEXT_TOP;
    sanim->text.end_x = ANIM_TEXT_RIGHT;
    sanim->text.end_y = ANIM_TEXT_BOTTOM;

    /* canvas obj #0: black fill */
    {
        struct seqcanvas_layer_t *c = &(sanim->canvas_obj[0]);
        c->rop.msetfill.h = ANIM_HEIGHT;
        c->rop.msetfill.c = 0x7;
        c->what = SEQCL_MSETFILL;
    }

    seqanim_set_redraw_everything_flag(sanim);

    while (seqanim_running(sanim)) {
        if (kbhit()) {
            c = getch();
            if (c == 27) break;
        }

        nowcount = read_timer_counter();

        seqanim_step(sanim,nowcount);
        seqanim_redraw(sanim);
    }

    seqanim_free(&sanim);
#undef ANIM_HEIGHT
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

    sin2048fps16_free();
    font_bmp_free(&arial_small);
    font_bmp_free(&arial_medium);
    font_bmp_free(&arial_large);
    dumbpack_close(&sorc_pack);

    unhook_irqs();
    restore_text_mode();

    return 0;
}

