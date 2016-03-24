
#include <stdint.h>

#include <hw/vga/vga.h>

struct v320x200x256_VGA_state {
	uint16_t		stride;			// bytes per scanline
	uint16_t		width,virt_width;	// video width vs virtual width
	uint16_t		height,virt_height;	// video height vs virtual height
	uint16_t		scan_height;		// raster video height
	uint32_t		draw_offset;		// draw offset
	uint32_t		vis_offset;		// visual offset
	uint32_t		vram_size;		// size of VRAM
	uint8_t			scan_height_div;
	uint8_t			hpel;			// horizontal pel
	uint8_t			stride_shift;		// reflects VGA CRTC byte/word/dword mode
	uint8_t			_reserved:6;
	VGA_RAM_PTR		draw_ptr;
	VGA_RAM_PTR		vis_ptr;
	unsigned int		dar_n,dar_d;		// display aspect ratio
	unsigned int		par_n,par_d;		// pixel aspect ratio
};

extern struct v320x200x256_VGA_state v320x200x256_VGA_state;
extern struct vga_mode_params v320x200x256_VGA_crtc_state;
extern struct vga_mode_params v320x200x256_VGA_crtc_state_init;
extern struct vga_mode_params v320x200x256_VGA_crtc_state_basemode;

void v320x200x256_VGA_init();
void v320x200x256_VGA_update_par();
void v320x200x256_VGA_update_from_CRTC_state();
void v320x200x256_VGA_setmode(unsigned int flags);
#define v320x200x256_VGA_setmode_FLAG_DONT_USE_INT10		(1U << 0UL)
/* ^ set this flag if you already set the video mode to INT 10h mode 0x13 */

double v320x200x256_VGA_get_hsync_rate();
double v320x200x256_VGA_get_refresh_rate();
void v320x200x256_VGA_setwindow(int x,int y,int w,int h,int vwidth);
int v320x200x256_VGA_setvideomode(int width,int height,double hsync,double vsync);

static inline void v320x200x256_VGA_update_vis_ptr() {
#if TARGET_MSDOS == 16
	uint32_t o = vga_state.vga_ram_base + v320x200x256_VGA_state.vis_offset + v320x200x256_VGA_state.hpel;
	v320x200x256_VGA_state.vis_ptr = MK_FP(o >> 4UL,o & 0xFUL);
#else
	v320x200x256_VGA_state.vis_ptr = vga_state.vga_graphics_ram + v320x200x256_VGA_state.vis_offset + v320x200x256_VGA_state.hpel;
#endif
}

static inline void v320x200x256_VGA_update_draw_ptr() {
#if TARGET_MSDOS == 16
	uint32_t o = vga_state.vga_ram_base + v320x200x256_VGA_state.draw_offset;
	v320x200x256_VGA_state.draw_ptr = MK_FP(o >> 4UL,o & 0xFUL);
#else
	v320x200x256_VGA_state.draw_ptr = vga_state.vga_graphics_ram + v320x200x256_VGA_state.draw_offset;
#endif
}

static inline
#if TARGET_MSDOS == 16
unsigned char far *
#else
unsigned char *
#endif
v320x200x256_VGA_pixelmemaddro(const uint16_t o) {
	return v320x200x256_VGA_state.draw_ptr+o;
}

static inline
#if TARGET_MSDOS == 16
unsigned char far *
#else
unsigned char *
#endif
v320x200x256_VGA_pixelmemaddrxy(const unsigned int x,const unsigned int y) {
	const uint16_t o = ((uint16_t)y * (uint16_t)v320x200x256_VGA_state.stride) + (uint16_t)x;
	return v320x200x256_VGA_pixelmemaddro(o);
}

static inline uint8_t v320x200x256_VGA_getpixelnc(const unsigned int x,const unsigned int y) {
	return *v320x200x256_VGA_pixelmemaddrxy(x,y);
}

static inline uint8_t v320x200x256_VGA_getpixel(const unsigned int x,const unsigned int y) {
	if (x >= v320x200x256_VGA_state.stride || y >= v320x200x256_VGA_state.height) return 0;
	return v320x200x256_VGA_getpixelnc(x,y);
}

static inline void v320x200x256_VGA_setpixelnc(const unsigned int x,const unsigned int y,const uint8_t v) {
	*v320x200x256_VGA_pixelmemaddrxy(x,y) = v;
}

static inline void v320x200x256_VGA_setpixel(const unsigned int x,const unsigned int y,const uint8_t v) {
	if (x >= v320x200x256_VGA_state.stride || y >= v320x200x256_VGA_state.height) return;
	v320x200x256_VGA_setpixelnc(x,y,v);
}

