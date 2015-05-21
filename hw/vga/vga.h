
#ifndef __DOSLIB_HW_VGA_VGA_H
#define __DOSLIB_HW_VGA_VGA_H

#include <hw/cpu/cpu.h>
#include <stdint.h>

#if TARGET_MSDOS == 32
typedef unsigned char *VGA_RAM_PTR;
typedef uint16_t *VGA_ALPHA_PTR;
#else
typedef unsigned char far *VGA_RAM_PTR;
typedef uint16_t far *VGA_ALPHA_PTR;
#endif

/* vga_flags */
#define VGA_IS_TANDY			0x02	/* Tandy/PCjr */
#define VGA_IS_HGC			0x04
#define VGA_IS_MCGA			0x08
#define VGA_IS_VGA			0x10
#define VGA_IS_EGA			0x20
#define VGA_IS_CGA			0x40
#define VGA_IS_MDA			0x80
#define VGA_IS_AMSTRAD			0x100

/* sequencer index */
#define VGA_AC_ENABLE			0x20

/* graphics controller regs */
#define VGA_GC_DATA_ROTATE_OP_NONE		(0 << 3)
#define VGA_GC_DATA_ROTATE_OP_AND		(1 << 3)
#define VGA_GC_DATA_ROTATE_OP_OR		(2 << 3)
#define VGA_GC_DATA_ROTATE_OP_XOR		(3 << 3)

/* font manipulation vars */
#define VGA_EGA_BYTES_PER_CHAR_BITMAP		32
#define VGA_EGA_BYTES_PER_CHAR_BITMAP_SHIFT	5

/* EGA/VGA memory map bits */
enum {
	VGA_MEMMAP_A0000_128K=0,
	VGA_MEMMAP_A0000_64K,
	VGA_MEMMAP_B0000_32K,
	VGA_MEMMAP_B8000_32K
};

enum {
	VGA_CGA_MODE_40WIDE=0,
	VGA_CGA_MODE_80WIDE=0x01,
	VGA_CGA_MODE_GRAPHICS=0x02,
	VGA_CGA_MODE_BW=0x04,
	VGA_CGA_MODE_VIDEO_ENABLE=0x08,
	VGA_CGA_MODE_VIDEO_640=0x10,
	VGA_CGA_MODE_NO_BLINKING=0x20
};

/* sequencer registers */
enum {
	VGA_SC_MAP_MASK=2
};

/* graphics controller registers */
enum {
	VGA_GC_ENABLE_SET_RESET=0,
	VGA_GC_SET_RESET=1,
	VGA_GC_DATA_ROTATE=3,
	VGA_GC_MODE=5,
	VGA_GC_BIT_MASK=8
};

enum { /* CGA palette selection (320x200x4) */
	VGA_CGA_PALETTE_GR_BR_RD=0,
	VGA_CGA_PALETTE_CY_MA_WH=1
};

enum { /* color select (text=border color  320x200=background    640x200=foreground */
	VGA_CGA_PALETTE_CS_BLUE=(1U << 0U),
	VGA_CGA_PALETTE_CS_GREEN=(1U << 1U),
	VGA_CGA_PALETTE_CS_RED=(1U << 2U),
	VGA_CGA_PALETTE_CS_INTENSITY=(1U << 3U),
	VGA_CGA_PALETTE_CS_ALT_INTENSITY=(1U << 4U)
};

extern VGA_RAM_PTR	vga_graphics_ram;
extern VGA_RAM_PTR	vga_graphics_ram_fence;
extern VGA_ALPHA_PTR	vga_alpha_ram;
extern VGA_ALPHA_PTR	vga_alpha_ram_fence;

extern uint32_t		vga_clock_rates[4];
extern unsigned char	vga_pos_x,vga_pos_y,vga_color;
extern unsigned char	vga_width,vga_height;
extern unsigned char	vga_alpha_mode;
extern unsigned char	vga_hgc_type;
extern unsigned int	vga_base_3x0;
extern unsigned long	vga_ram_base;
extern unsigned long	vga_ram_size;
extern unsigned char	vga_stride;
extern unsigned short	vga_flags;
extern unsigned char	vga_9wide;

unsigned char int10_getmode();
void vga_write(const char *msg);
void int10_setmode(unsigned char mode);
void update_state_vga_memory_map_select(unsigned char c);
unsigned char vga_force_read(const VGA_RAM_PTR p);
void vga_force_write(VGA_RAM_PTR p,const unsigned char c);
void vga_force_write_w(VGA_ALPHA_PTR p,const unsigned short c);
int check_vga_3C0();
void vga_sync_hw_cursor();
void vga_sync_bios_cursor();
void update_state_vga_memory_map_select(unsigned char c);
void vga_set_memory_map(unsigned char c);
void vga_bios_set_80x50_text();
void update_state_from_vga();
int probe_vga();
void vga_write_state_DEBUG(FILE *f);
void vga_relocate_crtc(unsigned char color);
void vga_switch_to_mono();
void vga_switch_to_color();
void vga_turn_on_hgc();
void vga_turn_off_hgc();
void vga_set_cga_palette_and_background(unsigned char pal,unsigned char color);
void vga_set_cga_mode(unsigned char b);
void vga_tandy_setpalette(unsigned char i,unsigned char c);
void vga_enable_256color_modex();
void vga_set_stride(unsigned int stride);
void vga_set_start_location(unsigned int offset);
void vga_set_ypan_sub(unsigned char c);
void vga_set_xpan(unsigned char c);
void vga_splitscreen(unsigned int v);
void vga_alpha_switch_to_font_plane();
void vga_alpha_switch_from_font_plane();
void vga_set_9wide(unsigned char en);
void vga_select_charset_a_b(unsigned short a,unsigned short b);
void vga_write_crtc_mode(struct vga_mode_params *p,unsigned int flags);
void vga_correct_crtc_mode(struct vga_mode_params *p);
void vga_read_crtc_mode(struct vga_mode_params *p);

#define VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC	0x0001

static inline unsigned char vga_read_CRTC(unsigned char i) {
	outp(vga_base_3x0+4,i);
	return inp(vga_base_3x0+5);
}

static inline unsigned char vga_read_GC(unsigned char i) {
	outp(0x3CE,i);
	return inp(0x3CF);
}

static inline unsigned char vga_read_sequencer(unsigned char i) {
	outp(0x3C4,i);
	return inp(0x3C5);
}

static inline void vga_write_sequencer(unsigned char i,unsigned char c) {
	outp(0x3C4,i);
	outp(0x3C5,c);
}

static inline void vga_write_GC(unsigned char i,unsigned char c) {
	outp(0x3CE,i);
	outp(0x3CF,c);
}

static inline void vga_write_CRTC(unsigned char i,unsigned char c) {
	outp(vga_base_3x0+4,i);
	outp(vga_base_3x0+5,c);
}

static inline void vga_write_color(unsigned char c) {
	vga_color = c;
}

static inline void vga_read_PAL(unsigned char i,unsigned char *p,unsigned int count) {
	count *= 3;
	outp(0x3C7,i);
	while (count-- > 0) *p++ = inp(0x3C9);
}

static inline void vga_write_PAL(unsigned char i,unsigned char *p,unsigned int count) {
	count *= 3;
	outp(0x3C8,i);
	while (count-- > 0) outp(0x3C9,*p++);
}

/* NTS: VGA hardware treats bit 5 of the index as a "screen enable".
 *      When the caller is done reprogramming it is expected to or the index by VGA_AC_ENABLE */
static inline void vga_write_AC(unsigned char i,unsigned char c) {
	inp(vga_base_3x0+0xA); /* reset flipflop */
	outp(0x3C0,i);
	outp(0x3C0,c);
	inp(vga_base_3x0+0xA);
}

static inline unsigned char vga_read_AC(unsigned char i) {
	unsigned char c;

	/* NTS: Reading the palette registers must occur this way because
	 *      an old S3 Virge DX card I have will misread the values
	 *      when PAS=1 otherwise. */

	inp(vga_base_3x0+0xA);	/* reset latch */
	outp(0x3C0,i&(~0x20));	/* index with PAS=0 */
	c = inp(0x3C1);
	inp(vga_base_3x0+0xA);	/* reset latch */
	outp(0x3C0,i|0x20);	/* index with PAS=1 */
	inp(vga_base_3x0+0xA);	/* reset latch */

	return c;
}

static inline void vga_AC_reenable_screen() {
	inp(vga_base_3x0+0xA); /* reset flipflop */
	outp(0x3C0,0x20);
	inp(vga_base_3x0+0xA);
}

static inline void vga_palette_lseek(unsigned int i) {
	outp(0x3C8,i);
}

static inline void vga_palette_write(unsigned char r,unsigned char g,unsigned char b) {
	outp(0x3C9,r);
	outp(0x3C9,g);
	outp(0x3C9,b);
}

static inline unsigned char vga_in_vsync() {
	unsigned int p = vga_base_3x0 + 0xA;
	return (inp(p) & 0x8);
}

static inline void vga_wait_for_hsync() {
	unsigned int p = vga_base_3x0 + 0xA;
	while ((inp(p) & 0x1) == 0);
}

static inline void vga_wait_for_hsync_end() {
	unsigned int p = vga_base_3x0 + 0xA;
	while ((inp(p) & 0x1) != 0);
}

static inline void vga_wait_for_vsync() {
	unsigned int p = vga_base_3x0 + 0xA;
	while ((inp(p) & 0x8) == 0);
}

static inline void vga_wait_for_vsync_end() {
	unsigned int p = vga_base_3x0 + 0xA;
	while ((inp(p) & 0x8) != 0);
}

static inline unsigned char vga_AC_RGB_to_code(unsigned char r,unsigned char g,unsigned char b) {
	return	(((b>>1)&1)<<0)|(((g>>1)&1)<<1)|(((r>>1)&1)<<2)|
		(((b>>0)&1)<<3)|(((g>>0)&1)<<4)|(((r>>0)&1)<<5);
}

#if defined(TARGET_WINDOWS) && TARGET_MSDOS == 32
/* we have to call the Win16 executable buffer */
#else
# pragma aux int10_getmode = \
	"mov	ax,0x0F00" \
	"int	0x10" \
	value [al] \
	modify [ax bx];

# pragma aux int10_setmode = \
	"xor	ah,ah" \
	"int	0x10" \
	parm [al] \
	modify [ax bx];
#endif

/* Watcom loves to reorder memory I/O on us. So to avoid problems we just have to
 * suck it up and write the VGA memory functions in assembly language where it can't
 * reorder access. Keeping the I/O in order is very important if we're to make
 * proper use of the VGA hardware latch */
#if TARGET_MSDOS == 32
#pragma aux vga_force_read = \
	"mov	al,[ebx]" \
	modify [al] \
	value [al] \
	parm [ebx];
#pragma aux vga_force_write = \
	"mov	[ebx],al" \
	parm [ebx] [al];
#else
#pragma aux vga_force_read = \
	"mov	al,es:[di]" \
	modify [al] \
	value [al] \
	parm [es di];
#pragma aux vga_force_write = \
	"mov	es:[di],al" \
	parm [es di] [al];
#pragma aux vga_force_write_w = \
	"mov	es:[di],ax" \
	parm [es di] [ax];
#endif

/* all video timing and mode related values.
 * notice that other params like graphics controller settings aren't included here,
 * since you can always set those up yourself without disturbing the video timing */
/* NTS: VGA standard hardware seems to be designed so that generally clock_div2=1
 *      means you divide the master clock by 2. But it also seems that this bit is
 *      ignored in 256-color modes and the VGA acts like it's always clear (NOT SET),
 *      timings are programmed as if 640x400, and pixels are doubled horizontally
 *      regardless of the bit. Despite all that, the general mode timing calculations
 *      work out correctly anyway. This is important to know, because it may affect
 *      your ability to correctly use this API for major mode changes between
 *      256-color modes & other modes. */
struct vga_mode_params {
	unsigned char		clock_select:2;	/* 0x3C2 Misc out reg bits 2-3 */
	unsigned char		hsync_neg:1;	/* 0x3C2 bit 6 */
	unsigned char		vsync_neg:1;	/* 0x3C2 bit 7 */
	unsigned char		clock9:1;	/* sequencer[1] bit 0 */
	unsigned char		clock_div2:1;	/* sequencer[1] bit 3 [divide master clock by 2] */
	unsigned char		word_mode:1;	/* CRTC[0x17] bit 6 = WORD MODE */
	unsigned char		dword_mode:1;	/* CRTC[0x14] bit 6 = DWORD MODE */
	unsigned short		horizontal_total; /* CRTC[0]             -5 */
	unsigned short		horizontal_display_end; /* CRTC[1]       -1 */
	unsigned short		horizontal_blank_start; /* CRTC[2] */
	unsigned short		horizontal_blank_end;   /* CRTC[3] bit 0-4 & CRTC[5] bit 7 */
	unsigned short		horizontal_start_retrace;/* CRTC[4] */
	unsigned short		horizontal_end_retrace;	/* CRTC[5] bit 0-4 */
	unsigned char		horizontal_start_delay_after_total; /* CRTC[3] bit 5-6 */
	unsigned char		horizontal_start_delay_after_retrace; /* CRTC[5] bit 5-6 */
	unsigned short		vertical_total;	/* CRTC[6] and CRTC[7] bit 0 and CRTC[7] bit 5          -2 */
	unsigned short		vertical_start_retrace; /* CRTC[0x10] and CRTC[7] bit 2 and CRTC[7] bit 7 */
	unsigned short		vertical_end_retrace; /* CRTC[0x11] bit 0-3 */
	unsigned char		refresh_cycles_per_scanline; /* CRTC[0x11] bit 6 */
	unsigned char		inc_mem_addr_only_every_4th; /* CRTC[0x14] bit 5 */
	unsigned short		vertical_display_end;	/* CRTC[0x12] and CRTC[7] bit 1 and CRTC[7] bit 6 */
	unsigned short		vertical_blank_start;	/* CRTC[0x15] and CRTC[7] bit 3 */
	unsigned short		vertical_blank_end;	/* CRTC[0x16] bit 0-7 */
	unsigned char		shift_load_rate:1;	/* sequencer[1] bit 2 */
	unsigned char		shift4_enable:1;	/* sequencer[1] bit 4 */
	unsigned char		address_wrap_select:1;	/* CRTC[0x17] bit 5 */
	unsigned char		memaddr_div2:1;		/* CRTC[0x17] bit 3 */
	unsigned char		scanline_div2:1;	/* CRTC[0x17] bit 2 */
	unsigned char		map14:1;		/* CRTC[0x17] bit 1 */
	unsigned char		map13:1;		/* CRTC[0x17] bit 0 */
	unsigned char		scan_double:1;		/* CRTC[0x09] bit 7 */
	unsigned char		max_scanline;		/* CRTC[0x09] bit 4-0 */
	unsigned char		offset;			/* CRTC[0x13] */
	unsigned char		sync_enable:1;		/* CRTC[0x17] bit 7 */
};

#endif /* __DOSLIB_HW_VGA_VGA_H */

