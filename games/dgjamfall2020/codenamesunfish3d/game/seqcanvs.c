
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
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
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
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"

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

static void seqanim_text_color(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    if (e->param1 == 0) {
        sa->text.def_palcolor[0] = sa->text.def_palcolor[1] = sa->text.def_palcolor[2] = 0xFFu;
    }
    else {
        sa->text.def_palcolor[0] = (unsigned char)(e->param1 >> 16ul);
        sa->text.def_palcolor[1] = (unsigned char)(e->param1 >>  8ul);
        sa->text.def_palcolor[2] = (unsigned char)(e->param1 >>  0ul);
    }
}

static void seqanim_text_clear(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    (void)e; // unused

    sa->text.msg = NULL;
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
    if (sa->text.font == arial_small)
        return 10;

    return 0;
}

static void seqanim_step_text(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    uint32_t c;

    (void)e; // unused

    if (sa->text.msg == NULL)
        sa->text.msg = sa->events->params;

again:
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
            case 0x11:
                if (*(sa->text.msg) == 0) break; /* even as a two-byte sequence please don't allow 0x00 to avoid confusion with end of string */
                switch (*(sa->text.msg)) {
                    case 0x20: // arial medium
                        sa->text.font = arial_medium;
                        (sa->text.msg)++;
                        break;
                    case 0x21: // small arial
                        sa->text.font = arial_small;
                        (sa->text.msg)++;
                        break;
                    case 0x22: // small arial
                        sa->text.font = arial_large;
                        (sa->text.msg)++;
                        break;
                };
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

        if (sa->flags & SEQAF_USER_HURRY_UP)
            goto again;
    }
    else {
        if (sa->flags & SEQAF_USER_HURRY_UP) {
            sa->next_event = sa->current_time;
            sa->flags &= ~SEQAF_USER_HURRY_UP;
        }

        (sa->events)++; /* next */
    }
}

static void seqanim_step_text_fadein(struct seqanim_t *sa,const struct seqanim_event_t *e) {
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

    sa->flags &= ~SEQAF_USER_HURRY_UP;
    sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;
    if (sa->text.palcolor[0] == color[0] && sa->text.palcolor[1] == color[1] && sa->text.palcolor[2] == color[2]) {
        (sa->events)++; /* next */
        sa->next_event = sa->current_time;
    }
    else {
        sa->next_event++;
    }
}

static void seqanim_step_text_fadeout(struct seqanim_t *sa,const struct seqanim_event_t *e) {
    const unsigned char sub = (e->param1 != 0) ? e->param1 : 8;
    unsigned int i;

    for (i=0;i < 3;i++) {
        if (sa->text.palcolor[i] >= sub)
            sa->text.palcolor[i] -= sub;
        else
            sa->text.palcolor[i] = 0;
    }

    sa->flags &= ~SEQAF_USER_HURRY_UP;
    sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;
    if ((sa->text.palcolor[0] | sa->text.palcolor[1] | sa->text.palcolor[2]) == 0) {
        (sa->events)++; /* next */
        sa->next_event = sa->current_time;
    }
    else {
        sa->next_event++;
    }
}

void seqanim_set_time(struct seqanim_t *sa,const uint32_t t) {
    sa->current_time = t;
}

void seqanim_hurryup(struct seqanim_t *sa) {
    sa->flags |= SEQAF_USER_HURRY_UP;
    sa->next_event = sa->current_time;
}

void seqanim_step(struct seqanim_t *sa) {
    while (sa->current_time >= sa->next_event) {
        if (sa->events == NULL) {
            sa->flags |= SEQAF_END;
        }
        else {
            const struct seqanim_event_t *e = sa->events;

            switch (sa->events->what) {
                case SEQAEV_END:
                    sa->next_event = sa->current_time;
                    sa->flags |= SEQAF_END;
                    return;
                case SEQAEV_TEXT_CLEAR:
                    sa->next_event = sa->current_time;
                    seqanim_text_clear(sa,e);
                    (sa->events)++; /* next */
                    break;
                case SEQAEV_TEXT_COLOR:
                    seqanim_text_color(sa,e);
                    (sa->events)++; /* next */
                    break;
                case SEQAEV_TEXT:
                    seqanim_step_text(sa,e); /* will advance sa->events */
                    break;
                case SEQAEV_TEXT_FADEOUT:
                    seqanim_step_text_fadeout(sa,e);
                    break;
                case SEQAEV_TEXT_FADEIN:
                    seqanim_step_text_fadein(sa,e);
                    break;
                case SEQAEV_WAIT:
                    if (sa->flags & SEQAF_USER_HURRY_UP)
                        sa->next_event = sa->current_time;
                    else
                        sa->next_event += e->param1;

                    (sa->events)++; /* next */
                    break;
                case SEQAEV_PAUSE:
                    sa->next_event = sa->current_time+1u; /* to break the loop */
                    (sa->events)++; /* next */
                    break;
                case SEQAEV_CALLBACK:
                    if (sa->events->params != NULL)
                        ((seqcl_callback_funcptr)(sa->events->params))(sa,sa->events);
                    else
                        (sa->events)++; /* next */
                    break;
                default:
                    (sa->events)++; /* next */
                    break;
            }
        }
    }

    if (sa->canvas_obj != NULL) {
        struct seqcanvas_layer_t *cl;
        unsigned int i;

        for (i=0;i < sa->canvas_obj_count;i++) {
            cl = &(sa->canvas_obj[i]);
            if (cl->what == SEQCL_VRL) {
                seq_com_vrl_anim_movement(sa,cl);
                if (cl->rop.vrl.anim.anim_callback != NULL) {
                    uint32_t pt;

                    while (sa->current_time >= cl->rop.vrl.anim.next_frame) {
                        pt = cl->rop.vrl.anim.next_frame;
                        cl->rop.vrl.anim.anim_callback(sa,cl);
                        if (pt >= cl->rop.vrl.anim.next_frame) break; // break now if no change or backwards
                    }
                }
            }
        }
    }
}

static void seqanim_draw_canvasobj_msetfill(struct seqanim_t *sa,struct seqcanvas_layer_t *cl) {
    (void)sa;

    if (cl->rop.msetfill.h != 0) {
        const uint16_t w = (uint16_t)cl->rop.msetfill.c + ((uint16_t)cl->rop.msetfill.c << 8u);
        vga_write_sequencer(0x02/*map mask*/,0xF);
        vga_rep_stosw(vga_state.vga_graphics_ram,w,((320u/4u)*cl->rop.msetfill.h)/2u);
    }
}

static void seqanim_draw_canvasobj_text(struct seqanim_t *sa,struct seqcanvas_layer_t *cl) {
    (void)sa;

    if (cl->rop.text.textcdef != NULL && cl->rop.text.font != NULL) {
        int x = cl->rop.text.x,y = cl->rop.text.y;
        unsigned int i;

        for (i=0;i < cl->rop.text.textcdef_length;i++)
            x = font_bmp_draw_chardef_vga8u(cl->rop.text.font,cl->rop.text.textcdef[i],x,y,cl->rop.text.color);
    }
}

static void seqanim_draw_canvasobj_rotozoom(struct seqanim_t *sa,struct seqcanvas_layer_t *cl) {
    (void)sa;

    if (cl->rop.rotozoom.imgseg != 0 && cl->rop.rotozoom.h != 0) {
        rotozoomer_fast_effect(320/*width*/,cl->rop.rotozoom.h/*height*/,cl->rop.rotozoom.imgseg,sa->current_time - cl->rop.rotozoom.time_base);

        /* unless time_base == (~0ul) rotozoomer always redraws */
        if (cl->rop.rotozoom.time_base != (~0ul))
            sa->flags |= SEQAF_REDRAW;
    }
}

static void seqanim_draw_canvasobj_vrl(struct seqanim_t *sa,struct seqcanvas_layer_t *cl) {
    (void)sa;

    if (cl->rop.vrl.vrl != NULL) {
        if (cl->rop.vrl.anim.flags & SEQANF_HFLIP) {
            draw_vrl1_vgax_modexclip_hflip(cl->rop.vrl.x,cl->rop.vrl.y,
                    cl->rop.vrl.vrl->vrl_header,
                    cl->rop.vrl.vrl->vrl_lineoffs,
                    cl->rop.vrl.vrl->buffer+sizeof(*(cl->rop.vrl.vrl->vrl_header)),
                    cl->rop.vrl.vrl->bufsz-sizeof(*(cl->rop.vrl.vrl->vrl_header)));
        }
        else {
            draw_vrl1_vgax_modexclip(cl->rop.vrl.x,cl->rop.vrl.y,
                cl->rop.vrl.vrl->vrl_header,
                cl->rop.vrl.vrl->vrl_lineoffs,
                cl->rop.vrl.vrl->buffer+sizeof(*(cl->rop.vrl.vrl->vrl_header)),
                cl->rop.vrl.vrl->bufsz-sizeof(*(cl->rop.vrl.vrl->vrl_header)));
        }
    }
}

static void seqanim_draw_canvasobj_bitblt(struct seqanim_t *sa,struct seqcanvas_layer_t *cl) {
    unsigned char far * const ovga = vga_state.vga_graphics_ram;
    const uint16_t sst = cl->rop.bitblt.src_step;
    const uint16_t dst = cl->rop.bitblt.dst_step;
    const uint16_t len = cl->rop.bitblt.length;
    uint16_t rc = cl->rop.bitblt.rows;
    uint16_t s = cl->rop.bitblt.src;
    uint16_t d = cl->rop.bitblt.dst + FP_OFF(vga_state.vga_graphics_ram);

    (void)sa;

    vga_state.vga_graphics_ram = MK_FP(0xA000,0x0000);

    vga_write_sequencer(0x02/*map mask*/,0xF);
    vga_setup_wm1_block_copy();
    while (rc-- != 0u) {
        vga_wm1_mem_block_copy(d,s,len);
        s += sst;
        d += dst;
    }
    vga_restore_rm0wm0();

    vga_state.vga_graphics_ram = ovga;
}

static void seqanim_draw_canvasobj(struct seqanim_t *sa,struct seqcanvas_layer_t *cl) {
    switch (cl->what) {
        case SEQCL_MSETFILL:
            seqanim_draw_canvasobj_msetfill(sa,cl);
            break;
        case SEQCL_ROTOZOOM:
            seqanim_draw_canvasobj_rotozoom(sa,cl);
            break;
        case SEQCL_VRL:
            seqanim_draw_canvasobj_vrl(sa,cl);
            break;
        case SEQCL_CALLBACK:
            if (cl->rop.callback.fn != NULL)
                cl->rop.callback.fn(sa,cl);
            break;
        case SEQCL_TEXT:
            seqanim_draw_canvasobj_text(sa,cl);
            break;
        case SEQCL_BITBLT:
            seqanim_draw_canvasobj_bitblt(sa,cl);
            break;
        case SEQCL_NONE:
        default:
            break;
    }
}

void seqanim_draw(struct seqanim_t *sa) {
    sa->flags &= ~SEQAF_REDRAW;

    if (sa->canvas_obj != NULL) {
        unsigned int i;
        for (i=0;i < sa->canvas_obj_count;i++)
            seqanim_draw_canvasobj(sa,&(sa->canvas_obj[i]));
    }
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

