
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

struct seqanim_t {
    /* what to draw (back to front) */
    unsigned int                    canvas_obj_alloc;   /* how much is allocated */
    unsigned int                    canvas_obj_count;   /* how much to draw */
    struct seqcanvas_layer_t*       canvas_obj;
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
    return sa;
}

void seqanim_free(struct seqanim_t **sa) {
    if (*sa != NULL) {
        seqanim_free_canvas(*sa);
        free(*sa);
        *sa = NULL;
    }
}

/*---------------------------------------------------------------------------*/
/* introduction sequence                                                     */
/*---------------------------------------------------------------------------*/

void seq_intro(void) {
    struct seqanim_t *sanim;

    if ((sanim=seqanim_alloc()) == NULL)
        fatal("seqanim");

    seqanim_free(&sanim);
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

