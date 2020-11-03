
/* alternate page offsets big enough for a 352x232 mode */
#define VGA_GAME_PAGE_FIRST         0x0000
#define VGA_GAME_PAGE_SECOND        0x5000

static inline int use_adlib() {
    return (adlib_fm_voices != 0u);
}

void game_normal_setup(void);
void free_sound_blaster_dma(void);
int realloc_sound_blaster_dma(const unsigned buffer_size);
void woo_menu_item_coord(unsigned int *x,unsigned int *y,unsigned int i);
void woo_menu_item_draw_char(unsigned int o,unsigned char c,unsigned char color);
void woo_menu_item_drawstr(unsigned int x,unsigned int y,const char *str,unsigned char color);
void sound_blaster_stop_playback(void);
void sound_blaster_hook_irq(void);
void game_spriteimg_free(void);
void my_unhook_irq(void);
void gen_res_free(void);
void load_def_pal(void);

extern unsigned char FAR*                       vga_8x8_font_ptr;

extern struct dma_8237_allocation*              sound_blaster_dma;
extern struct sndsb_ctx*                        sound_blaster_ctx;

extern unsigned char                            sound_blaster_stop_on_irq;
extern unsigned char                            sound_blaster_irq_hook;
extern unsigned char                            sound_blaster_old_irq_masked;

