
#include <hw/vga/vrl.h>

#define MAX_RTIMG                   2
#define MAX_VRLIMG                  64
#define MAX_VRAMIMG                 2

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
    VRAMIMG_TWNCNTR                 /* exterior "town center" shot 320x168 */
};

struct seq_com_vramimg_state {
    uint16_t            vramoff;
};

struct seq_com_rotozoom_state {
    unsigned            imgseg;
    unsigned            rzoom_index;
};

struct seq_com_vrl_image_state {
    struct vrl_image        vrl;
};

extern struct seq_com_vramimg_state seq_com_vramimg[MAX_VRAMIMG];
extern struct seq_com_rotozoom_state seq_com_rotozoom_image[MAX_RTIMG];
extern struct seq_com_vrl_image_state seq_com_vrl_image[MAX_VRLIMG];

extern unsigned int seq_com_anim_h;

void seq_com_cleanup(void);
void seq_com_load_rotozoom(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_put_solidcolor(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_put_rotozoom(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_init_mr_woo(struct seqanim_t *sa,const struct seqanim_event_t *ev);
int seq_com_load_vrl_from_dumbpack(const unsigned vrl_slot,struct dumbpack * const pack,const unsigned packoff,const unsigned char paloff);
int seq_com_load_pal_from_dumbpack(const unsigned char pal,struct dumbpack * const pack,const unsigned packoff);
void seq_com_load_mr_woo_anim(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_vrl_anim_cb(struct seqanim_t *sa,struct seqcanvas_layer_t *ca);
void seq_com_vrl_anim_movement(struct seqanim_t *sa,struct seqcanvas_layer_t *ca);
void seq_com_put_mr_woo_anim(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_put_nothing(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_load_vram_image(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_put_vram_image(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_load_games_chars(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_save_palette(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_fadein_saved_palette(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_begin_anim_move(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_do_anim_move(struct seqanim_t *sa,const struct seqanim_event_t *ev);
void seq_com_put_mr_vrl(struct seqanim_t *sa,const struct seqanim_event_t *ev);

