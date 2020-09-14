
#define SEQANF_ANIMATE              (1u << 0u)
#define SEQANF_PINGPONG             (1u << 1u)
#define SEQANF_REVERSE              (1u << 2u)
#define SEQANF_HFLIP                (1u << 3u)

#define SEQAF_REDRAW                (1u << 0u)          /* redraw the canvas and page flip to screen */
#define SEQAF_END                   (1u << 1u)
#define SEQAF_TEXT_PALCOLOR_UPDATE  (1u << 2u)
#define SEQAF_USER_HURRY_UP         (1u << 3u)          /* this is set if the user hits a key to speed up the dialogue */

/* param1 of SEQAEV_TEXT_CLEAR */
#define SEQAEV_TEXT_CLEAR_FLAG_NOPALUPDATE (1u << 0u)

/* param1 of SEQAEV_TEXT */
#define SEQAEV_TEXT_FLAG_NOWAIT         (1u << 0u)

/* sequence animation engine (for VGA page flipping animation) */
enum seqcanvas_layer_what {
    SEQCL_NONE=0,                                       /* 0   nothing */
    SEQCL_MSETFILL,                                     /* 1   msetfill */
    SEQCL_ROTOZOOM,                                     /* 2   rotozoom */
    SEQCL_VRL,                                          /* 3   vrl */
    SEQCL_CALLBACK,                                     /* 4   callback */

    SEQCL_TEXT,                                         /* 5   text */
    SEQCL_BITBLT,                                       /* 6   bitblt (copy one VRAM location to another) */
    SEQCL_MAX                                           /* 7   */
};

enum {
    SEQAEV_END=0,                   /* end of sequence */
    SEQAEV_TEXT_CLEAR,              /* clear/reset/home text */
    SEQAEV_TEXT_COLOR,              /* change text (palette) color on clear */
    SEQAEV_TEXT,                    /* print text (UTF-8 string pointed to by 'params') with control codes embedded as well */
    SEQAEV_TEXT_FADEOUT,            /* fade out (palette entry for) text. param1 is how much to subtract from R/G/B. At 120Hz a value of 255 is just over 2 seconds. 0 means use default. */
    SEQAEV_TEXT_FADEIN,             /* fade in to RGB 888 in param2, or if param2 == 0 to default palette color. param1 same as FADEOUT */
    SEQAEV_WAIT,                    /* pause for 'param1' tick counts */
    SEQAEV_PAUSE,                   /* pause (such as to present contents on screen) */
    SEQAEV_CALLBACK,                /* custom event (callback funcptr 'params') */

    SEQAEV_MAX
};

union seqcanvas_layeru_t;
struct seqcanvas_layer_t;

struct seqcanvas_anim_t {
    void                (*anim_callback)(struct seqanim_t *sa,struct seqcanvas_layer_t *ca);
    uint32_t            next_frame;
    unsigned int        frame_delay;
    int                 cur_frame;
    int                 min_frame;
    int                 max_frame;
    unsigned char       flags;

    /* movement */
    int                 base_x,base_y;
    int                 move_x,move_y;
    unsigned int        move_duration;
    uint32_t            move_base;
};

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
    struct seqcanvas_anim_t         anim;
};

struct seqcanvas_callback {
    void                            (*fn)(struct seqanim_t *sa,struct seqcanvas_layer_t *layer);
    uint32_t                        param1,param2;
};

struct seqcanvas_text {
    unsigned int                    textcdef_length;
    uint16_t*                       textcdef;           /* text to draw (as chardef indexes) */
    struct font_bmp*                font;               /* font to use */
    int                             x,y;                /* where to start drawing */
    unsigned char                   color;
};

/* in unchained 256-color mode, VGA write mode 1 and copying from offscreen RAM can be an efficient means to bitblt (4 pixels at once per R/W!) */
/* NOTE: If the image fills the screen horizontally and matches the stride of the screen, rows==1 and length=rows*height is perfectly legitimate */
struct seqcanvas_bitblt {
    uint16_t                        src,dst;            /* src, dest video ram locations. This is unchained 256-color mode, therefore multiple of 4 pixels */
    uint16_t                        length;             /* how much to copy per row (4-pixel planar byte) */
    uint16_t                        src_step;           /* source stride for copy (4-pixel planar byte) */
    uint16_t                        dst_step;           /* dest stride for copy (4-pixel planar byte) */
    uint16_t                        rows;               /* rows */
};

union seqcanvas_layeru_t {
    struct seqcanvas_memsetfill     msetfill;           /* memset (solid color) */
    struct seqcanvas_rotozoom       rotozoom;           /* draw a rotozoomer */
    struct seqcanvas_vrl            vrl;                /* draw VRL image */
    struct seqcanvas_callback       callback;           /* callback function */
    struct seqcanvas_text           text;               /* draw text */
    struct seqcanvas_bitblt         bitblt;             /* bitblt (copy VRAM to another VRAM location) */
};

struct seqcanvas_layer_t {
    unsigned char                   what;
    union seqcanvas_layeru_t        rop;
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
    uint32_t                        current_time;
    uint32_t                        next_event;         /* counter value */
    /* events to process, provided by caller (not allocated) */
    const struct seqanim_event_t*   events;
    /* text print (dialogue) state */
    struct seqanim_text_t           text;
    /* state flags */
    unsigned int                    flags;
};

typedef void (*seqcl_callback_funcptr)(struct seqanim_t *sa,const struct seqanim_event_t *ev);

static inline int seqanim_running(struct seqanim_t *sa) {
    if (sa->flags & SEQAF_END)
        return 0;

    return 1;
}

static inline void seqanim_set_redraw_everything_flag(struct seqanim_t *sa) {
    sa->flags |= SEQAF_REDRAW | SEQAF_TEXT_PALCOLOR_UPDATE;
}

void seq_com_vrl_anim_movement(struct seqanim_t *sa,struct seqcanvas_layer_t *ca);
void seqcanvas_text_free_text(struct seqcanvas_text *t);
int seqcanvas_text_alloc_text(struct seqcanvas_text *t,unsigned int len);
void seqcanvas_clear_layer(struct seqcanvas_layer_t *l);
int seqanim_alloc_canvas(struct seqanim_t *sa,unsigned int max);
void seqanim_free_canvas(struct seqanim_t *sa);
struct seqanim_t *seqanim_alloc(void);
void seqanim_free(struct seqanim_t **sa);
void seqanim_text_color(struct seqanim_t *sa,const struct seqanim_event_t *e);
void seqanim_text_clear(struct seqanim_t *sa,const struct seqanim_event_t *e);
unsigned int seqanim_text_height(struct seqanim_t *sa);
void seqanim_set_time(struct seqanim_t *sa,const uint32_t t);
void seqanim_hurryup(struct seqanim_t *sa);
void seqanim_step(struct seqanim_t *sa);
void seqanim_draw_canvasobj_msetfill(struct seqanim_t *sa,struct seqcanvas_layer_t *cl);
void seqanim_draw_canvasobj_text(struct seqanim_t *sa,struct seqcanvas_layer_t *cl);
void seqanim_draw_canvasobj_rotozoom(struct seqanim_t *sa,struct seqcanvas_layer_t *cl);
void seqanim_draw_canvasobj_vrl(struct seqanim_t *sa,struct seqcanvas_layer_t *cl);
void seqanim_draw_canvasobj_bitblt(struct seqanim_t *sa,struct seqcanvas_layer_t *cl);
void seqanim_draw_canvasobj(struct seqanim_t *sa,struct seqcanvas_layer_t *cl);
void seqanim_draw(struct seqanim_t *sa);
void seqanim_update_text_palcolor(struct seqanim_t *sa);
void seqanim_redraw(struct seqanim_t *sa);

