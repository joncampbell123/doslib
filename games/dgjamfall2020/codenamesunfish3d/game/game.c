
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

#define MAX_RTIMG           2
#define MAX_VRLIMG          64

#define SORC_PAL_OFFSET             0x40

/* 0-8 anim1 9 frames */
#define SORC_VRL_ANIM1_OFFSET       (0x20+0)/*0x20*/

/* 9 still "uhhhh" frame */
#define SORC_VRL_STILL_UHH          (0x20+9)/*0x29*/

/* 10-18 anim2 9 frames */
#define SORC_VRL_ANIM2_OFFSET       (0x20+10)/*0x2A*/

#define SORC_VRL_END                (0x20+19)/*0x33*/

#define SORC_VRL_GAMESCHARS_VRLBASE (0x33+0)/*0x33*/
#define SORC_VRL_GAMECHRMOUTH_BASE  (0x37+0)/*0x37*/
#define SORC_VRL_GAMECHROHCOMEON    (0x39+0)/*0x39*/

/* rotozoomer images (256x256) */
enum {
    RZOOM_NONE=0,       /* i.e. clear the slot and free memory */
    RZOOM_WXP,
    RZOOM_ATPB
};

/* vram images (used for static images that do not change, loaded into unused VRAM rather than system RAM) */
enum {
    VRAMIMG_TMPLIE,                 /* interior "temple" shot 320x168 */
    VRAMIMG_TWNCNTR,                /* exterior "town center" shot 320x168 */

    VRAMIMG_MAX
};

struct seq_com_vramimg_state {
    uint16_t            vramoff;
};

struct seq_com_vramimg_state seq_com_vramimg[VRAMIMG_MAX] = { {0} };

unsigned int seq_com_anim_h = 0;

struct seq_com_rotozoom_state {
    unsigned            imgseg;
    unsigned            rzoom_index;
};

struct seq_com_rotozoom_state seq_com_rotozoom_image[MAX_RTIMG] = { {0,0} };

struct seq_com_vrl_image_state {
    struct vrl_image        vrl;
};

struct seq_com_vrl_image_state seq_com_vrl_image[MAX_VRLIMG] = { { NULL,0 } };

void seq_com_cleanup(void) {
    unsigned int i;

    for (i=0;i < MAX_RTIMG;i++)
        rotozoomer_imgfree(&(seq_com_rotozoom_image[i].imgseg));
    for (i=0;i < MAX_VRLIMG;i++)
        free_vrl(&(seq_com_vrl_image[i].vrl));
}

/* param1: what
 * param2: slot */
void seq_com_load_rotozoom(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seq_com_rotozoom_state *rs;

    /* catch errors */
    if (ev->param2 >= MAX_RTIMG) fatal("rotozoom image index out of range");
    rs = &seq_com_rotozoom_image[ev->param2];

    if (ev->param1 == RZOOM_NONE) {
        rotozoomer_imgfree(&(rs->imgseg));
    }
    else {
        if (rs->imgseg == 0) {
            if ((rs->imgseg=rotozoomer_imgalloc()) == 0)
                fatal("rotozoom image fail to allocate");
        }

        if (rs->rzoom_index != (unsigned)ev->param1) {
            switch (ev->param1) {
                case RZOOM_WXP:
                    if (rotozoomerpngload(rs->imgseg,"wxpbrz.png",0))
                        fatal("wxpbrz.png");
                    break;
                case RZOOM_ATPB:
                    if (rotozoomerpngload(rs->imgseg,"atmpbrz.png",0))
                        fatal("atmpbrz.png");
                    break;
                default:
                    fatal("rotozoom image unknown image code");
                    break;
            };

            /* loading eats time depending on how fast DOS and the underlying storage device are.
             * It's even possible some weirdo will run this off a 1.44MB floppy. */
            sa->next_event = sa->current_time = read_timer_counter();
        }
    }

    rs->rzoom_index = (unsigned)ev->param1;
    (sa->events)++; /* next */
}

/* param1: color
 * param2: canvas layer */
void seq_com_put_solidcolor(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    if (sa->canvas_obj_count <= ev->param2)
        sa->canvas_obj_count = ev->param2+1u;

    /* change to rotozoomer */
    co->what = SEQCL_MSETFILL;
    co->rop.msetfill.h = seq_com_anim_h;
    co->rop.msetfill.c = (ev->param1 & 0xFFu) | ((ev->param1 & 0xFFu) << 8u);

    /* changed canvas, make redraw */
    sa->flags |= SEQAF_REDRAW;

    (sa->events)++; /* next */
}

/* param1: slot
 * param2: canvas layer */
void seq_com_put_rotozoom(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seq_com_rotozoom_state *rs;
    struct seqcanvas_layer_t* co;

    /* catch errors */
    if (ev->param1 >= MAX_RTIMG) fatal("rotozoom image index out of range");
    rs = &seq_com_rotozoom_image[ev->param1];

    if (rs->imgseg == 0) fatal("attempt to place unallocated rotozoom");

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    if (sa->canvas_obj_count <= ev->param2)
        sa->canvas_obj_count = ev->param2+1u;

    /* change to rotozoomer */
    co->what = SEQCL_ROTOZOOM;
    co->rop.rotozoom.imgseg = rs->imgseg;
    co->rop.rotozoom.time_base = sa->next_event;
    co->rop.rotozoom.h = seq_com_anim_h;

    /* changed canvas, make redraw */
    sa->flags |= SEQAF_REDRAW;

    (sa->events)++; /* next */
}

void seq_com_init_mr_woo(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    uint32_t ofs;

    (void)sa;
    (void)ev;

    if (sorc_pack_open())
        fatal("mr_woo_init");
    if ((ofs=dumbpack_ent_offset(sorc_pack,0)) == 0ul)
        fatal("mr_woo_init");
    if (lseek(sorc_pack->fd,ofs,SEEK_SET) != ofs)
        fatal("mr_woo_init");
    if (read(sorc_pack->fd,common_tmp_small,32*3) != (32*3)) // 32 colors
        fatal("mr_woo_init");

    pal_buf_to_vga(/*offset*/SORC_PAL_OFFSET,/*count*/32,common_tmp_small);

    (sa->events)++; /* next */
}

int seq_com_load_vrl_from_dumbpack(const unsigned vrl_slot,struct dumbpack * const pack,const unsigned packoff,const unsigned char paloff) {
    uint32_t ofs,sz;

    if (vrl_slot >= MAX_VRLIMG)
        fatal("load_vrl_slot out of range");

    if ((ofs=dumbpack_ent_offset(pack,packoff)) == 0ul)
        return 1;
    if ((sz=dumbpack_ent_size(pack,packoff)) == 0ul)
        return 1;
    if (lseek(pack->fd,ofs,SEEK_SET) != ofs)
        return 1;
    if (load_vrl_fd(&(seq_com_vrl_image[vrl_slot].vrl),pack->fd,sz) != 0)
        return 1;

    vrl_palrebase(
        seq_com_vrl_image[vrl_slot].vrl.vrl_header,
        seq_com_vrl_image[vrl_slot].vrl.vrl_lineoffs,
        seq_com_vrl_image[vrl_slot].vrl.buffer+sizeof(*(seq_com_vrl_image[vrl_slot].vrl.vrl_header)),
        paloff);

    return 0;
}

int seq_com_load_pal_from_dumbpack(const unsigned char pal,struct dumbpack * const pack,const unsigned packoff) {
    uint32_t ofs,sz;
    unsigned int i;

    if ((ofs=dumbpack_ent_offset(pack,packoff)) == 0ul)
        return 1;
    if ((sz=dumbpack_ent_size(pack,packoff)) == 0ul)
        return 1;
    if (lseek(pack->fd,ofs,SEEK_SET) != ofs)
        return 1;
    if (sz > sizeof(common_tmp_small))
        return 1;
    if (read(pack->fd,common_tmp_small,sz) != sz)
        return 1;

    sz /= 3;
    vga_palette_lseek(pal);
    for (i=0;i < (unsigned int)sz;i++)
        vga_palette_write(common_tmp_small[(i*3)+0]>>2,common_tmp_small[(i*3)+1]>>2,common_tmp_small[(i*3)+2]>>2);

    return 0;
}

/* param1: which anim sequence
 *    0 = anim1
 *    1 = uhhhh
 *    2 = anim2
 * param2: flags
 *    bit 0: required load
 *    bit 1: unload */
void seq_com_load_mr_woo_anim(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    unsigned vrl_slot,vrl_count,packoff;
    struct dumbpack *pack;

    (void)sa;

    if (sorc_pack_open())
        fatal("mr_woo_init");

    switch (ev->param1) {
        case 0://anim1
            packoff = 2;
            pack = sorc_pack;
            vrl_slot = SORC_VRL_ANIM1_OFFSET;
            vrl_count = 9;
            break;
        case 1://uhhhhh
            packoff = 2+9;
            pack = sorc_pack;
            vrl_slot = SORC_VRL_STILL_UHH;
            vrl_count = 1;
            break;
        case 2://anim2
            packoff = 2+9+1;
            pack = sorc_pack;
            vrl_slot = SORC_VRL_ANIM2_OFFSET;
            vrl_count = 9;
            break;
        default:
            fatal("load_mr_woo_anim out of range");
            break;
    };

    while (vrl_count != 0u) {
        if (ev->param2 & 2u) { /* unload */
            if (vrl_slot < MAX_VRLIMG)
                free_vrl(&(seq_com_vrl_image[vrl_slot].vrl));
        }
        else {
            if (seq_com_load_vrl_from_dumbpack(vrl_slot,pack,packoff,SORC_PAL_OFFSET)) {
                if (ev->param2 & 1u) /* required load */
                    goto fatalload;
            }
        }

        vrl_count--;
        vrl_slot++;
        packoff++;
    }

    /* loading eats time depending on how fast DOS and the underlying storage device are.
     * It's even possible some weirdo will run this off a 1.44MB floppy. */
    sa->next_event = sa->current_time = read_timer_counter();

    (sa->events)++; /* next */
    return;
fatalload:
    fatal("sorcwooloadanim");
}

void seq_com_vrl_anim_cb(struct seqanim_t *sa,struct seqcanvas_layer_t *ca) {
    if (ca->rop.vrl.anim.flags & SEQANF_ANIMATE) {
        if (ca->rop.vrl.anim.flags & SEQANF_REVERSE) {
            if ((--ca->rop.vrl.anim.cur_frame) < ca->rop.vrl.anim.min_frame) {
                ca->rop.vrl.anim.cur_frame = ca->rop.vrl.anim.min_frame + 1;
                ca->rop.vrl.anim.flags &= ~SEQANF_REVERSE;
            }
        }
        else {
            if ((++ca->rop.vrl.anim.cur_frame) > ca->rop.vrl.anim.max_frame) {
                ca->rop.vrl.anim.cur_frame = ca->rop.vrl.anim.max_frame - 1;
                ca->rop.vrl.anim.flags |= SEQANF_REVERSE;
            }
        }

        sa->flags |= SEQAF_REDRAW;
        ca->rop.vrl.vrl = &seq_com_vrl_image[ca->rop.vrl.anim.cur_frame].vrl;
        ca->rop.vrl.anim.next_frame += ca->rop.vrl.anim.frame_delay;
    }
}

void seq_com_vrl_anim_movement(struct seqanim_t *sa,struct seqcanvas_layer_t *ca) {
    if (ca->rop.vrl.anim.move_duration != 0u) {
        int32_t rel = (int32_t)(sa->current_time - ca->rop.vrl.anim.move_base); /* to make signed multiply easier in next steps */
        unsigned char eof = 0;
        int dx,dy;

        if (rel > (int32_t)ca->rop.vrl.anim.move_duration) {
            rel = (int32_t)ca->rop.vrl.anim.move_duration;
            eof = 1;
        }
        else {
            sa->flags |= SEQAF_REDRAW;
            if (rel < 0l) rel = 0l;
        }

        dx = (int)((rel * ca->rop.vrl.anim.move_x) / (int32_t)ca->rop.vrl.anim.move_duration);
        dy = (int)((rel * ca->rop.vrl.anim.move_y) / (int32_t)ca->rop.vrl.anim.move_duration);

        ca->rop.vrl.x = ca->rop.vrl.anim.base_x + dx;
        ca->rop.vrl.y = ca->rop.vrl.anim.base_y + dy;

        if (eof) ca->rop.vrl.anim.move_duration = 0;
    }
}

/* param1: animation
 * param2: canvas layer */
void seq_com_put_mr_woo_anim(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    (void)sa;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    if (sa->canvas_obj_count <= ev->param2)
        sa->canvas_obj_count = ev->param2+1u;

    co->what = SEQCL_VRL;
    co->rop.vrl.x = 70;
    co->rop.vrl.y = 0;

    switch (ev->param1) {
        case 0://anim1
            co->rop.vrl.anim.anim_callback = seq_com_vrl_anim_cb;
            co->rop.vrl.anim.frame_delay = 120 / 15;
            co->rop.vrl.anim.cur_frame = co->rop.vrl.anim.min_frame = SORC_VRL_ANIM1_OFFSET;
            co->rop.vrl.anim.max_frame = SORC_VRL_ANIM1_OFFSET + 8;
            co->rop.vrl.anim.flags = SEQANF_PINGPONG | SEQANF_ANIMATE;
            break;
        case 1://uhhhhh
            co->rop.vrl.anim.anim_callback = seq_com_vrl_anim_cb;
            co->rop.vrl.anim.frame_delay = 32767;
            co->rop.vrl.anim.cur_frame = co->rop.vrl.anim.min_frame = co->rop.vrl.anim.max_frame = SORC_VRL_STILL_UHH;
            co->rop.vrl.anim.flags = 0;
            break;
        case 2://anim2
            co->rop.vrl.anim.anim_callback = seq_com_vrl_anim_cb;
            co->rop.vrl.anim.frame_delay = 120 / 30;
            co->rop.vrl.anim.cur_frame = co->rop.vrl.anim.min_frame = SORC_VRL_ANIM2_OFFSET;
            co->rop.vrl.anim.max_frame = SORC_VRL_ANIM2_OFFSET + 8;
            co->rop.vrl.anim.flags = SEQANF_PINGPONG | SEQANF_ANIMATE;
            break;
        default:
            fatal("put_mr_woo_anim out of range");
            break;
    };

    sa->flags |= SEQAF_REDRAW;
    co->rop.vrl.vrl = &seq_com_vrl_image[co->rop.vrl.anim.cur_frame].vrl;
    co->rop.vrl.anim.next_frame = sa->current_time + co->rop.vrl.anim.frame_delay;

    (sa->events)++; /* next */
}

/* param1: animation
 * param2: canvas layer */
void seq_com_put_nothing(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    (void)sa;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    co->what = SEQCL_NONE;

    sa->flags |= SEQAF_REDRAW;

    (sa->events)++; /* next */
}

/* param1: image | (palofs << 8u)
 * param2: vram offset */
void seq_com_load_vram_image(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    unsigned char palofs;
    struct minipng_reader *rdr;
    unsigned int imgw,imgh;
    unsigned char *row;
    const char *path;
    unsigned int i,x;
    uint16_t ofs,tof;

    (void)sa;

    switch (ev->param1&0xFFu) {
        case VRAMIMG_TMPLIE:
            path = "tmplie.png";
            imgw = 320;
            imgh = 168;
            break;
        case VRAMIMG_TWNCNTR:
            path = "twnctr.png";
            imgw = 320;
            imgh = 168;
            break;
        default:
            fatal("vram_image unknown index");
            break;
    }

    if (ev->param2 >= 0x10000ul || ((uint32_t)ev->param2+(((uint32_t)imgw/4ul)*(uint32_t)imgh)) > 0x10000ul)
        fatal("vram_image offset too large");

    ofs = (uint16_t)(ev->param2);
    palofs = (unsigned char)(ev->param1 >> 8u);
    seq_com_vramimg[ev->param1&0xFFu].vramoff = ofs;

    if ((rdr=minipng_reader_open(path)) == NULL)
        fatal("vram_image png error %s",path);
    if (minipng_reader_parse_head(rdr) || rdr->plte == NULL || rdr->plte_count == 0 || rdr->ihdr.width != imgw || rdr->ihdr.height != imgh || rdr->ihdr.bit_depth != 8)
        fatal("vram_image png error %s",path);
    if ((row=malloc(imgw)) == NULL)
        fatal("vram_image png error %s",path);

    {
        const unsigned char *p = (const unsigned char*)(rdr->plte);
        vga_palette_lseek(palofs);
        for (i=0;i < rdr->plte_count;i++)
            vga_palette_write(p[(i*3)+0]>>2,p[(i*3)+1]>>2,p[(i*3)+2]>>2);
    }

    for (i=0;i < imgh;i++) {
        minipng_reader_read_idat(rdr,row,1); /* pad byte */
        minipng_reader_read_idat(rdr,row,imgw); /* row */

        vga_write_sequencer(0x02/*map mask*/,0x01);
        for (x=0,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x] + palofs;

        vga_write_sequencer(0x02/*map mask*/,0x02);
        for (x=1,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x] + palofs;

        vga_write_sequencer(0x02/*map mask*/,0x04);
        for (x=2,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x] + palofs;

        vga_write_sequencer(0x02/*map mask*/,0x08);
        for (x=3,tof=ofs;x < imgw;x += 4u,tof++)
            *((unsigned char far*)MK_FP(0xA000,tof)) = row[x] + palofs;

        ofs += imgw/4u;
    }

    minipng_reader_close(&rdr);
    free(row);

    /* loading eats time depending on how fast DOS and the underlying storage device are.
     * It's even possible some weirdo will run this off a 1.44MB floppy. */
    sa->next_event = sa->current_time = read_timer_counter();

    (sa->events)++; /* next */
}

/* param1: vram image
 * param2: canvas layer */
void seq_com_put_vram_image(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    (void)sa;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    switch (ev->param1&0xFFu) {
        case VRAMIMG_TMPLIE:
        case VRAMIMG_TWNCNTR:
            co->rop.bitblt.src = seq_com_vramimg[ev->param1].vramoff;
            co->rop.bitblt.dst = 0;
            co->rop.bitblt.length = (320u/4u)*168u;
            co->rop.bitblt.rows = 1;
            co->rop.bitblt.src_step = 0;
            co->rop.bitblt.dst_step = 0;
            break;
        default:
            fatal("vram_image unknown index");
            break;
    }

    co->what = SEQCL_BITBLT;

    sa->flags |= SEQAF_REDRAW;

    (sa->events)++; /* next */
}

void seq_com_load_games_chars(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    (void)sa;
    (void)ev;

    if (sorc_pack_open())
        fatal("mr_woo_init");

    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMESCHARS_VRLBASE+0,sorc_pack,25,0xA0)) //gmch1.vrl
        goto fatalload;
    if (seq_com_load_pal_from_dumbpack(0xA0,sorc_pack,21)) //gmch1.pal
        goto fatalload;
    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMESCHARS_VRLBASE+1,sorc_pack,26,0xB0)) //gmch2.vrl
        goto fatalload;
    if (seq_com_load_pal_from_dumbpack(0xB0,sorc_pack,22)) //gmch2.pal
        goto fatalload;
    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMESCHARS_VRLBASE+2,sorc_pack,27,0xC0)) //gmch3.vrl
        goto fatalload;
    if (seq_com_load_pal_from_dumbpack(0xC0,sorc_pack,23)) //gmch3.pal
        goto fatalload;
    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMESCHARS_VRLBASE+3,sorc_pack,28,0xD0)) //gmch4.vrl
        goto fatalload;
    if (seq_com_load_pal_from_dumbpack(0xD0,sorc_pack,24)) //gmch4.pal
        goto fatalload;

    /* mouth (shares palette with gmch1.vrl) */
    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMECHRMOUTH_BASE+0,sorc_pack,29,0xA0)) //gmchm1.vrl
        goto fatalload;
    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMECHRMOUTH_BASE+1,sorc_pack,30,0xA0)) //gmchm2.vrl
        goto fatalload;

    /* Oh come on! That's just mean! (gmch3oco.vrl same palette as gmch3.vrl) */
    if (seq_com_load_vrl_from_dumbpack(SORC_VRL_GAMECHROHCOMEON+0,sorc_pack,31,0xC0))
        goto fatalload;

    /* loading eats time depending on how fast DOS and the underlying storage device are.
     * It's even possible some weirdo will run this off a 1.44MB floppy. */
    sa->next_event = sa->current_time = read_timer_counter();

    (sa->events)++; /* next */
    return;
fatalload:
    fatal("gameschars");
}

uint32_t seq_com_pal_anim_base;

/* param1: flags
 *   bit 0: 1=blank palette after saving
 * param2: what to save  (start | (count << 16u)) */
void seq_com_save_palette(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    unsigned int start,count;
    unsigned int i;

    start = (unsigned int)(ev->param2 & 0xFFu);
    count = (unsigned int)((ev->param2 >> 16u) & 0x1FFu);
    if ((start+count) > 256)
        fatal("save_palette out of range");

    seq_com_pal_anim_base = sa->current_time;

    vga_read_PAL(start,common_tmp_small+start,count);

    if (ev->param1 & 0x1u) {
        vga_palette_lseek(start);
        for (i=0;i < count;i++) vga_palette_write(0,0,0);
        sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;
    }

    (sa->events)++; /* next */
}

/* param1: how fast to fade */
/* param2: flags
 *  bit 0: fade out */
void seq_com_fadein_saved_palette(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    unsigned int m,em,i;

    (void)ev;

    {
        uint32_t r = (sa->current_time - seq_com_pal_anim_base) * (ev->param1 ? ev->param1 : 8);
        if (r > 256ul) r = 256ul;

        if (ev->param2 & 1u) {
            m = 256u - (unsigned int)r;
            em = 0;
        }
        else {
            m = (unsigned int)r;
            em = 256;
        }
    }

    vga_palette_lseek(0);
    for (i=0;i < 256;i++) {
        vga_palette_write(
            (unsigned char)(((unsigned int)common_tmp_small[(i*3u)+0u]*m)>>8u),
            (unsigned char)(((unsigned int)common_tmp_small[(i*3u)+1u]*m)>>8u),
            (unsigned char)(((unsigned int)common_tmp_small[(i*3u)+2u]*m)>>8u));
    }

    sa->next_event = sa->current_time + 1u;
    sa->flags |= SEQAF_TEXT_PALCOLOR_UPDATE;

    if (m == em)
        (sa->events)++; /* next */
}

/* param1:
 * param2: canvas layer */
void seq_com_begin_anim_move(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    (void)sa;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    if (co->what != SEQCL_VRL) fatal("anim move when not VRL");

    co->rop.vrl.anim.base_x = co->rop.vrl.x;
    co->rop.vrl.anim.base_y = co->rop.vrl.y;
    co->rop.vrl.anim.move_x = 0;
    co->rop.vrl.anim.move_y = 0;
    co->rop.vrl.anim.move_duration = 0;
    co->rop.vrl.anim.move_base = sa->current_time;

    (sa->events)++; /* next */
}

/* param1: x | (y << 10) | (duration << 20ul)
 * param2: canvas layer */
void seq_com_do_anim_move(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    (void)sa;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);

    if (co->what != SEQCL_VRL) fatal("anim move when not VRL");

    co->rop.vrl.anim.move_x = (ev->param1 & 0x3FFul);
    if (co->rop.vrl.anim.move_x & 0x200)
        co->rop.vrl.anim.move_x -= 0x400;

    co->rop.vrl.anim.move_y = ((ev->param1 >> 10ul) & 0x3FFul);
    if (co->rop.vrl.anim.move_y & 0x200)
        co->rop.vrl.anim.move_y -= 0x400;

    co->rop.vrl.anim.move_duration = ((ev->param1 >> 20ul) & 0x3FFul);

    (sa->events)++; /* next */
}

/* param1: vrl image | (x << 10) | (y << 20) | (hflip << 30)
 * param2: canvas layer */
void seq_com_put_mr_vrl(struct seqanim_t *sa,const struct seqanim_event_t *ev) {
    struct seqcanvas_layer_t* co;

    (void)sa;

    if (ev->param2 >= sa->canvas_obj_alloc) fatal("canvas obj index out of range");
    co = &(sa->canvas_obj[ev->param2]);
    if ((ev->param1 & 0x3FFul) >= MAX_VRLIMG) fatal("vrl index out of range");

    if (sa->canvas_obj_count <= ev->param2)
        sa->canvas_obj_count = ev->param2+1u;

    co->what = SEQCL_VRL;
    co->rop.vrl.x = (unsigned int)((ev->param1 >> 10ul) & 0x3FFul);
    co->rop.vrl.y = (unsigned int)((ev->param1 >> 20ul) & 0x3FFul);
    co->rop.vrl.vrl = &seq_com_vrl_image[ev->param1 & 0x3FFul].vrl;
    memset(&(co->rop.vrl.anim),0,sizeof(co->rop.vrl.anim));

    if (ev->param1 & (1ul << 30ul))
        co->rop.vrl.anim.flags |= SEQANF_HFLIP;

    switch (ev->param1 & 0x3FFul) {
        case SORC_VRL_GAMECHRMOUTH_BASE:
            co->rop.vrl.anim.frame_delay = 120 / 6;
            co->rop.vrl.anim.cur_frame = co->rop.vrl.anim.min_frame = SORC_VRL_GAMECHRMOUTH_BASE;
            co->rop.vrl.anim.max_frame = SORC_VRL_GAMECHRMOUTH_BASE + 1;
            co->rop.vrl.anim.flags |= SEQANF_PINGPONG | SEQANF_ANIMATE;
            co->rop.vrl.anim.anim_callback = seq_com_vrl_anim_cb;
            break;
    };

    sa->flags |= SEQAF_REDRAW;

    (sa->events)++; /* next */
}

void gen_res_free(void) {
    seq_com_cleanup();
    sin2048fps16_free();
    font_bmp_free(&arial_small);
    font_bmp_free(&arial_medium);
    font_bmp_free(&arial_large);
    dumbpack_close(&sorc_pack);
}

/*---------------------------------------------------------------------------*/
/* introduction sequence                                                     */
/*---------------------------------------------------------------------------*/

const struct seqanim_event_t seq_intro_events[] = {
//  what                    param1,     param2,     params
    {SEQAEV_CALLBACK,       RZOOM_NONE, 0,          (const char*)seq_com_load_rotozoom}, // clear rotozoomer slot 0
    {SEQAEV_CALLBACK,       0,          3,          (const char*)seq_com_load_games_chars}, // load games chars
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_solidcolor}, // canvas layer 0 (param2) solid fill 0 (param1)
    {SEQAEV_CALLBACK,       VRAMIMG_TMPLIE|(0x60ul<<8ul),0x8000ul,(const char*)seq_com_load_vram_image}, // load tmplie and offset by 0x60 load to 0x8000 in planar unchained VRAM for bitblt
    {SEQAEV_CALLBACK,       VRAMIMG_TWNCNTR|(0x80ul<<8ul),0xC000ul,(const char*)seq_com_load_vram_image}, // load tmplie and offset by 0x80 load to 0xC000 in planar unchained VRAM for bitblt
    {SEQAEV_CALLBACK,       VRAMIMG_TWNCNTR,0,      (const char*)seq_com_put_vram_image}, // take VRAMIMG_TWNCNTR (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       1,          (0x00ul | (0x100ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette and (param1) blank it too

    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+3ul)/*vrl*/)|(130ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),4,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+1ul)/*vrl*/)|(140ul/*x*/<<10ul)|(/*y*/76ul<<20ul)|(1ul/*hflip*/<<30ul),2,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(280ul/*x*/<<10ul)|(/*y*/70ul<<20ul)|(0ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_PAUSE,          0,          0,          NULL}, //let it render
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_fadein_saved_palette}, // fade in saved VGA palette

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Welcome one and all to a new day\nin this world."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "A day where we all mill about in this world\ndoing our thing as a society" "\x10\x30" "----"},
    // no fade out, interrupted speaking

    // mouth, talking, secondary char, replace on canvas with new position
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(112ul/*x*/<<10ul)|(/*y*/53ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0xFFFF00ul, 0,          NULL}, //RRGGBB yellow
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "But what is our purpose in this game?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Hm? What?"},
    {SEQAEV_WAIT,           120*1,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    // mouth, talking, secondary char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHROHCOMEON+0ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(112ul/*x*/<<10ul)|(/*y*/53ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0xFFFF00ul, 0,          NULL}, //RRGGBB yellow
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Our purpose? What is the story? The goal?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Good question! "},

    /* turn around, to enter temple */
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(280ul/*x*/<<10ul)|(/*y*/70ul<<20ul)|(1ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(300ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT,           0,          0,          "I'll ask "},

    /* begins moving */
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       (40ul/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),1,(const char*)seq_com_do_anim_move},
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       (40ul/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),5,(const char*)seq_com_do_anim_move},

    {SEQAEV_TEXT,           0,          0,          "the programmer."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},

    {SEQAEV_CALLBACK,       0,          (0x00ul | (0x100ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette and (param1)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_fadein_saved_palette}, // fade out saved VGA palette

    /* walk to a door. screen fades out, fades in to room with only the one person. */

    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_nothing}, // clear canvas layer 1
    {SEQAEV_CALLBACK,       0,          2,          (const char*)seq_com_put_nothing}, // clear canvas layer 2
    {SEQAEV_CALLBACK,       0,          3,          (const char*)seq_com_put_nothing}, // clear canvas layer 3
    {SEQAEV_CALLBACK,       0,          4,          (const char*)seq_com_put_nothing}, // clear canvas layer 4
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5
    {SEQAEV_PAUSE,          0,          0,          NULL}, // render

    {SEQAEV_CALLBACK,       RZOOM_WXP,  0,          (const char*)seq_com_load_rotozoom}, // load rotozoomer slot 0 (param2) with 256x256 Windows XP background (param1)
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_init_mr_woo}, // initialize code and data for Mr. Wooo Sorcerer (load palette) (Future dev: param1 select func)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_load_mr_woo_anim}, // load anim1 (required param2==1)
    {SEQAEV_CALLBACK,       1,          1,          (const char*)seq_com_load_mr_woo_anim}, // load uhhh (required param2==1)
    {SEQAEV_CALLBACK,       2,          1,          (const char*)seq_com_load_mr_woo_anim}, // load anim2 (required param2==1)
    {SEQAEV_CALLBACK,       VRAMIMG_TMPLIE,0,       (const char*)seq_com_put_vram_image}, // take VRAMIMG_TMPLIE (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       1,          (0x00ul | (0x60ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette new colors and (param1) blank it too. 0x00-0x3F new rotozoomer, 0x40-0x5F Mr. Wooo

    {SEQAEV_PAUSE,          0,          0,          NULL}, // render

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(120ul/*x*/<<10ul)|(/*y*/135ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_fadein_saved_palette}, // fade in saved VGA palette
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Hello, games programmer?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    /* cut to Mr. Woo Sorcerer in front of a demo effect */

    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_rotozoom}, // put rotozoomer slot 0 (param1) to canvas layer 0 (param2)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_mr_woo_anim}, // put anim1 (param1) to canvas layer 1 (param2)
    {SEQAEV_TEXT_COLOR,     0x00FFFFul, 0,          NULL}, //RRGGBB cyan
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "I am a super awesome programmer! I write\nclever highly optimized code! Woooooooo!"},
    {SEQAEV_WAIT,           120*3,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_nothing}, // clear canvas layer 1
    {SEQAEV_CALLBACK,       0,          2,          (const char*)seq_com_load_mr_woo_anim}, // unload anim1 (param2==2)

    /* game character and room */

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(120ul/*x*/<<10ul)|(/*y*/135ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_CALLBACK,       VRAMIMG_TMPLIE,0,       (const char*)seq_com_put_vram_image}, // take VRAMIMG_TMPLIE (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Oh super awesome games programmer.\nWhat is our purpose in this game?"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5. Hide the mouth so it doesn't look like the game froze mid-animation during load.
    {SEQAEV_PAUSE,          0,          0,          NULL},

    /* Mr. Woo Sorcerer, blank background, downcast */

    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_put_nothing}, // clear canvas layer 5
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_solidcolor}, // canvas layer 0 (param2) solid fill 0 (param1)
    {SEQAEV_CALLBACK,       1,          1,          (const char*)seq_com_put_mr_woo_anim}, // put "uhhhh" (param1) to canvas layer 1 (param2)
    {SEQAEV_TEXT_COLOR,     0x00FFFFul, 0,          NULL}, //RRGGBB cyan
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_CALLBACK,       RZOOM_ATPB, 0,          (const char*)seq_com_load_rotozoom}, // load rotozoomer slot 0 (param2) with 256x256 Second Reality "atomic playboy" (param1)
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Uhm" "\x10\x29" ".........."},
    // no fade out, abrupt jump to next part

    /* Begins waving hands, another demo effect appears */

    {SEQAEV_CALLBACK,       2,          1,          (const char*)seq_com_put_mr_woo_anim}, // put anim2 (param1) to canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       1,          2,          (const char*)seq_com_load_mr_woo_anim}, // unload uhhh (param2==2)
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_put_rotozoom}, // put rotozoomer slot 0 (param1) to canvas layer 0 (param2)
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "I am super awesome programmer. I write\nawesome optimized code! Wooooooooo!"},
    {SEQAEV_WAIT,           120*5,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          (0x00ul | (0x100ul << 16ul)),(const char*)seq_com_save_palette}, // save VGA palette and (param1)
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_fadein_saved_palette}, // fade out saved VGA palette
    {SEQAEV_CALLBACK,       VRAMIMG_TWNCNTR,0,      (const char*)seq_com_put_vram_image}, // take VRAMIMG_TWNCNTR (param1) put into canvas layer 0 (param2) via BitBlt
    {SEQAEV_CALLBACK,       RZOOM_NONE, 0,          (const char*)seq_com_load_rotozoom}, // slot 0 we're done with the rotozoomer, free it
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_put_nothing}, // clear canvas layer 1
    {SEQAEV_CALLBACK,       2,          2,          (const char*)seq_com_load_mr_woo_anim}, // unload anim2 (param2==2)

    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(325ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl},
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+3ul)/*vrl*/)|(130ul/*x*/<<10ul)|(/*y*/120ul<<20ul)|(1ul/*hflip*/<<30ul),4,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+1ul)/*vrl*/)|(140ul/*x*/<<10ul)|(/*y*/76ul<<20ul)|(1ul/*hflip*/<<30ul),2,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       (SORC_VRL_GAMESCHARS_VRLBASE/*vrl*/)|(320ul/*x*/<<10ul)|(/*y*/70ul<<20ul)|(0ul/*hflip*/<<30ul),1,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_PAUSE,          0,          0,          NULL},
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_save_palette}, // save nothing of the VGA palette but reset anim timer
    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_fadein_saved_palette}, // fade in saved VGA palette

    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       ((-40l&0x3FFul)/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),1,(const char*)seq_com_do_anim_move},
    {SEQAEV_CALLBACK,       0,          5,          (const char*)seq_com_begin_anim_move},
    {SEQAEV_CALLBACK,       ((-40l&0x3FFul)/*x*/)|(0/*y*/<<10ul)|(60ul/*duration ticks*/<<20ul),5,(const char*)seq_com_do_anim_move},

    /* game character returns outside */
    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Alright,\x01 there is no story. So I'll just make\none up to get things started. \x02\x11\x21*ahem*"},
    {SEQAEV_WAIT,           120/4,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "There once was a master programmer with\nincredible programming skills."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "But he also had an incredible ego and was\nan incredible perfectionist."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "So he spent his life writing incredibly\noptimized clever useless code."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Then one day he ate one too many nachos\nand died of a heart attack. The end."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    /* someone in the crowd */

    // mouth, talking, secondary char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHROHCOMEON+0ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(112ul/*x*/<<10ul)|(/*y*/53ul<<20ul)|(1ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0xFFFF00ul, 0,          NULL}, //RRGGBB yellow
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Oh come on!\x01 That's just mean!"},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    /* game character */

    // mouth, talking, main char
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMESCHARS_VRLBASE+2ul)/*vrl*/)|(100ul/*x*/<<10ul)|(/*y*/40ul<<20ul)|(1ul/*hflip*/<<30ul),3,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)
    {SEQAEV_CALLBACK,       ((SORC_VRL_GAMECHRMOUTH_BASE+0ul)/*vrl*/)|(285ul/*x*/<<10ul)|(/*y*/85ul<<20ul)|(0ul/*hflip*/<<30ul),5,(const char*)seq_com_put_mr_vrl}, // main game char canvas layer 1 (param2)

    {SEQAEV_TEXT_COLOR,     0,          0,          NULL}, //default
    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Okay... okay...\x01 he spends his life writing\nincredible useless optimized code."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "He creates whole OSes but cares not\nfor utility and end user."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "He creates whole game worlds, but cares\nnot for the inhabitants."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "So he lives to this day writing... \x01well...\x01\nnothing of significance. Just woo."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Since the creator of this game doesn't seem\nto care for us or the story, we're on our own."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "So let's go make our own mini games so that\nthis game has something the user can play."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "I made a 3D maze-like overworld that\nconnects them all together for the user."},
    {SEQAEV_WAIT,           120*2,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

    {SEQAEV_TEXT_CLEAR,     0,          0,          NULL},
    {SEQAEV_TEXT,           0,          0,          "Alright, let's get started!"},
    {SEQAEV_WAIT,           120*1,      0,          NULL},
    {SEQAEV_TEXT_FADEOUT,   0,          0,          NULL},

/* screen fades out. Hammering, sawing, construction noises. */

    {SEQAEV_CALLBACK,       0,          0,          (const char*)seq_com_save_palette}, // save nothing of the VGA palette but reset anim timer
    {SEQAEV_CALLBACK,       0,          1,          (const char*)seq_com_fadein_saved_palette}, // fade out saved VGA palette

    {SEQAEV_END}
};

void seq_intro(void) {
#define ANIM_HEIGHT         168
#define ANIM_TEXT_TOP       168
#define ANIM_TEXT_LEFT      5
#define ANIM_TEXT_RIGHT     310
#define ANIM_TEXT_BOTTOM    198
    struct seqanim_t *sanim;
    int c;

    seq_com_anim_h = ANIM_HEIGHT;

    /* if we load this now, seqanim can automatically use it */
    if (font_bmp_do_load_arial_small())
        fatal("arial");
    if (font_bmp_do_load_arial_medium())
        fatal("arial");

    /* seqanim rotozoomer needs sin2048 */
    if (sin2048fps16_open())
        fatal("cannot open sin2048");

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

    sanim->canvas_obj_count = 1;

    /* canvas obj #0: black fill */
    {
        struct seqcanvas_layer_t *c = &(sanim->canvas_obj[0]);
        c->rop.msetfill.h = ANIM_HEIGHT;
        c->rop.msetfill.c = 0;
        c->what = SEQCL_MSETFILL;
    }

    seqanim_set_redraw_everything_flag(sanim);

    while (seqanim_running(sanim)) {
        if (kbhit()) {
            c = getch();
            if (c == 27)
                break;
            else if (c == ' ') {
                seqanim_set_time(sanim,read_timer_counter());
                seqanim_hurryup(sanim);
            }
        }

        seqanim_set_time(sanim,read_timer_counter());
        seqanim_step(sanim);
        seqanim_redraw(sanim);
    }

    seqanim_free(&sanim);
#undef ANIM_HEIGHT
}

/* ------------- */

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

    gen_res_free();
    check_heap();
    unhook_irqs();
    restore_text_mode();

    //debug
    dbg_heap_list();

    return 0;
}

