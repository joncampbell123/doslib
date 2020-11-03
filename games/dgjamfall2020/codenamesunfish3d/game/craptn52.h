
struct game_2d {
    int x,y;
};

#define NUM_TILES           256
#define TILES_VRAM_OFFSET   0xC000              /* enough for 4*16*256 */

#define GAME_VSCROLL        (1u << 0u)
#define GAME_HSCROLL        (1u << 1u)

#define NUM_SPRITEIMG       64

struct game_spriteimg {
    struct vrl_image        vrl;
};

extern struct game_spriteimg                    game_spriteimg[NUM_SPRITEIMG];

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

extern unsigned char                            game_sprite_max;
extern struct game_sprite                       game_sprite[NUM_SPRITES];

#define GAME_TILEMAP_DISPWIDTH  22
#define GAME_TILEMAP_WIDTH      ((22u*2u)+1u)
#define GAME_TILEMAP_HEIGHT     ((14u*2u)+1u)

extern unsigned char*                           game_tilemap;

extern unsigned char                            game_flags;
extern unsigned char                            game_scroll_mode;
extern unsigned int                             game_hscroll;
extern unsigned int                             game_vscroll;

void game_sprite_hide(unsigned i);
void game_sprite_show(unsigned i);
void game_sprite_imgset(unsigned i,unsigned img);
void game_sprite_position(unsigned i,unsigned int x,unsigned int y);
void game_sprite_clear(void);
void game_sprite_init(void);
void game_sprite_exit(void);
void game_spriteimg_freeimg(struct game_spriteimg *i);
void game_spriteimg_free(void);
void game_spriteimg_loadimg(struct game_spriteimg *i,const char *path);
void game_spriteimg_load(unsigned i,const char *path);
void game_normal_setup(void);
void game_noscroll_setup(void);
void game_vscroll_setup(void);
void game_hscroll_setup(void);
void load_tiles(uint16_t ofs,uint16_t ostride,const char *path);
void game_draw_tiles(unsigned x,unsigned y,unsigned w,unsigned h);
void game_draw_tiles_2pages(unsigned x,unsigned y,unsigned w,unsigned h);
void game_draw_sprite(unsigned x,unsigned y,unsigned simg,unsigned flags);
void game_update_sprites(void);
void game_tilecopy(unsigned x,unsigned y,unsigned w,unsigned h,const unsigned char *map);

/* alternate page offsets big enough for a 352x232 mode */
#define VGA_GAME_PAGE_FIRST         0x0000
#define VGA_GAME_PAGE_SECOND        0x5000

static inline int use_adlib() {
    return (adlib_fm_voices != 0u);
}

void game_normal_setup(void);
void free_sound_blaster_dma(void);
void sound_blaster_hook_irq(void);
void sound_blaster_unhook_irq(void);
void sound_blaster_stop_playback(void);
int realloc_sound_blaster_dma(const unsigned buffer_size);
void woo_menu_item_draw_char(unsigned int o,unsigned char c,unsigned char color);
void woo_menu_item_drawstr(unsigned int x,unsigned int y,const char *str,unsigned char color);
struct minipng_reader *woo_title_load_png(unsigned char *buf,unsigned int w,unsigned int h,const char *path);
void sound_blaster_stop_playback(void);
void sound_blaster_hook_irq(void);
void detect_sound_blaster(void);
void game_spriteimg_free(void);
void my_unhook_irq(void);
void gen_res_free(void);
void load_def_pal(void);
void woo_title(void);
int woo_menu(void);

void game_0(void);

extern unsigned char FAR*                       vga_8x8_font_ptr;

extern struct dma_8237_allocation*              sound_blaster_dma;
extern struct sndsb_ctx*                        sound_blaster_ctx;

extern unsigned char                            sound_blaster_stop_on_irq;
extern unsigned char                            sound_blaster_irq_hook;
extern unsigned char                            sound_blaster_old_irq_masked;

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

