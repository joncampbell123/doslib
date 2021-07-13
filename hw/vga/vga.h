
#ifndef __DOSLIB_HW_VGA_VGA_H
#define __DOSLIB_HW_VGA_VGA_H

#include <stdio.h>
#include <conio.h>

#include <hw/cpu/cpu.h>
#include <stdint.h>

#ifndef DOSLIB_REDEFINE_INP
# define DOSLIB_REDEFINE_INP
# include <hw/cpu/liteio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if TARGET_MSDOS == 32
typedef unsigned char *VGA_RAM_PTR;
typedef uint16_t *VGA_ALPHA_PTR;
#else
typedef unsigned char far *VGA_RAM_PTR;
typedef uint16_t far *VGA_ALPHA_PTR;
#endif

#pragma pack(push,1) // don't align structure members. we need the struct to be consistent even if other projects use different alignment
struct vgastate_t {
	unsigned char		vga_pos_x,vga_pos_y;		// 4 char = 4 byte align
	unsigned char		vga_hgc_type;
	unsigned char		vga_stride;
	uint16_t		vga_width,vga_height;		// 4 uint16_t = 8 byte align
	uint16_t		vga_base_3x0;
	uint16_t		vga_flags;
	uint32_t		vga_ram_base;			// 2 uint32_t = 8 byte align
	uint32_t		vga_ram_size;
	VGA_RAM_PTR		vga_graphics_ram;		// 4 far ptr = 16 byte align
	VGA_RAM_PTR		vga_graphics_ram_fence;
	VGA_ALPHA_PTR		vga_alpha_ram;
	VGA_ALPHA_PTR		vga_alpha_ram_fence;
	unsigned char		vga_color;			// 4 char (one is bitfield) = 4 byte align
	unsigned char		vga_draw_stride;
	unsigned char		vga_draw_stride_limit;		// further X clipping
	unsigned char		vga_alpha_mode:1;
	unsigned char		vga_9wide:1;
};
#pragma pack(pop)

extern struct vgastate_t	vga_state;

int probe_vga();
void update_state_from_vga();

static inline void vga_moveto(unsigned char x,unsigned char y) {
	vga_state.vga_pos_x = x;
	vga_state.vga_pos_y = y;
}

#if defined(TARGET_PC98)

/* emulated subset, for code migration */

static inline unsigned char int10_getmode(void) {
    return 3; /* think of the PC-98 text layer as perpetually locked into 80x25 mode */
}

static inline void int10_setmode(unsigned char mode) {
    (void)mode;
    // ignore
}

static inline unsigned char _color_vga2pc98(const unsigned char i) {
    /* input: VGA      I R G B
     * output: PC-98     G R B */
    return      ((i & 0x01/*B*/) ? 0x01 : 0x00) +
                ((i & 0x02/*G*/) ? 0x04 : 0x00) +
                ((i & 0x04/*R*/) ? 0x02 : 0x00);
}

static inline void vga_write_color(unsigned char c) {
    // translate VGA color to PC-98 attribute
    if ((c & 0xFu) != 0u) { /* foreground */
    	vga_state.vga_color = (_color_vga2pc98(c) << 5u) | 0x01u; /* map VGA [2:0] to PC-98 [7:5] */
    }
    else { /* background */
    	vga_state.vga_color = (_color_vga2pc98(c >> 4u) << 5u) | 0x05u; /* map VGA [6:4] to PC-98 [7:5], reverse */
    }

    if (c & 0x80u) /* blink */
        vga_state.vga_color |= 0x02u; /* PC-98 blink */
}

#else

/* vga_flags */
#define VGA_IS_TANDY			0x02
#define VGA_IS_HGC			0x04
#define VGA_IS_MCGA			0x08
#define VGA_IS_VGA			0x10
#define VGA_IS_EGA			0x20
#define VGA_IS_CGA			0x40
#define VGA_IS_MDA			0x80
#define VGA_IS_AMSTRAD			0x100
#define VGA_IS_PCJR             0x200

/* attribute controller index, OR with index to enable palette again */
#define VGA_AC_ENABLE			0x20

/* graphics controller regs */
#define VGA_GC_DATA_ROTATE_OP_NONE		(0 << 3)
#define VGA_GC_DATA_ROTATE_OP_AND		(1 << 3)
#define VGA_GC_DATA_ROTATE_OP_OR		(2 << 3)
#define VGA_GC_DATA_ROTATE_OP_XOR		(3 << 3)

/* font manipulation vars */
#define VGA_EGA_BYTES_PER_CHAR_BITMAP		32
#define VGA_EGA_BYTES_PER_CHAR_BITMAP_SHIFT	5

extern const unsigned char hgc_graphics_crtc[12];
extern const unsigned char hgc_text_crtc[12];

void vga_turn_on_hgc();
void vga_turn_off_hgc();

int tseng_et3000_detect();
void tseng_et4000_extended_set_lock(int x);
int tseng_et4000_detect();

/* EGA/VGA memory map bits */
enum {
	VGA_MEMMAP_A0000_128K =			0x0,
	VGA_MEMMAP_A0000_64K =			0x1,
	VGA_MEMMAP_B0000_32K =			0x2,
	VGA_MEMMAP_B8000_32K =			0x3
};

/* CGA mode byte */
enum {
	VGA_CGA_MODE_40WIDE =			0x00,
	VGA_CGA_MODE_80WIDE =			0x01,
	VGA_CGA_MODE_GRAPHICS =			0x02,
	VGA_CGA_MODE_BW =			0x04,
	VGA_CGA_MODE_VIDEO_ENABLE =		0x08,
	VGA_CGA_MODE_VIDEO_640 =		0x10,
	VGA_CGA_MODE_NO_BLINKING =		0x20
};

/* VGA CRTC registers */
enum {
	VGA_CRTC_HORIZONTAL_TOTAL =		0x00,
	VGA_CRTC_HORIZONTAL_DISPLAY_END =	0x01,
	VGA_CRTC_HORIZONTAL_BLANK_START =	0x02,
	VGA_CRTC_HORIZONTAL_BLANK_END =		0x03,
	VGA_CRTC_HORIZONTAL_RETRACE_START =	0x04,
	VGA_CRTC_HORIZONTAL_RETRACE_END =	0x05,
	VGA_CRTC_VERTICAL_TOTAL =		0x06,
	VGA_CRTC_OVERFLOW_REGISTER =		0x07,
	VGA_CRTC_PRESET_ROW_SCAN =		0x08,
	VGA_CRTC_MAXIMUM_SCAN_LINE =		0x09,
	VGA_CRTC_CURSOR_START =			0x0A,
	VGA_CRTC_CURSOR_END =			0x0B,
	VGA_CRTC_START_ADDRESS_HIGH =		0x0C,
	VGA_CRTC_START_ADDRESS_LOW =		0x0D,
	VGA_CRTC_CURSOR_LOCATION_HIGH =		0x0E,
	VGA_CRTC_CURSOR_LOCATION_LOW =		0x0F,
	VGA_CRTC_VERTICAL_RETRACE_START =	0x10,
	VGA_CRTC_VERTICAL_RETRACE_END =		0x11,
	VGA_CRTC_VERTICAL_DISPLAY_END =		0x12,
	VGA_CRTC_OFFSET =			0x13,
	VGA_CRTC_UNDERLINE_LOCATION =		0x14,
	VGA_CRTC_VERTICAL_BLANK_START =		0x15,
	VGA_CRTC_VERTICAL_BLANK_END =		0x16,
	VGA_CRTC_MODE_CONTROL =			0x17,
	VGA_CRTC_LINE_COMPARE =			0x18
};

/* attribute controller registers */
enum {
	VGA_AC_PALETTE_0 =			0x0,
	VGA_AC_PALETTE_1 =			0x1,
	VGA_AC_PALETTE_2 =			0x2,
	VGA_AC_PALETTE_3 =			0x3,
	VGA_AC_PALETTE_4 =			0x4,
	VGA_AC_PALETTE_5 =			0x5,
	VGA_AC_PALETTE_6 =			0x6,
	VGA_AC_PALETTE_7 =			0x7,
	VGA_AC_PALETTE_8 =			0x8,
	VGA_AC_PALETTE_9 =			0x9,
	VGA_AC_PALETTE_A =			0xA,
	VGA_AC_PALETTE_B =			0xB,
	VGA_AC_PALETTE_C =			0xC,
	VGA_AC_PALETTE_D =			0xD,
	VGA_AC_PALETTE_E =			0xE,
	VGA_AC_PALETTE_F =			0xF,
	VGA_AC_MODE_CONTROL =			0x10,
	VGA_AC_OVERSCAN_COLOR =			0x11,
	VGA_AC_COLOR_PLANE_ENABLE =		0x12,
	VGA_AC_HORIZONTAL_PIXEL_PANNING =	0x13,
	VGA_AC_COLOR_SELECT =			0x14
};
#define VGA_AC_PALETTE(x)			((x)&0xF)

/* sequencer registers */
enum {
	VGA_SC_RESET =				0x0,
	VGA_SC_CLOCK_MODE =			0x1,
	VGA_SC_MAP_MASK =			0x2,
	VGA_SC_CHARMAP_SELECT =			0x3,
	VGA_SC_MEM_MODE =			0x4
};

/* graphics controller registers */
enum {
	VGA_GC_SET_RESET =			0x0,
	VGA_GC_ENABLE_SET_RESET =		0x1,
	VGA_GC_COLOR_COMPARE =			0x2,
	VGA_GC_DATA_ROTATE =			0x3,
	VGA_GC_READ_MAP_SELECT =		0x4,
	VGA_GC_MODE =				0x5,
	VGA_GC_MISC_GRAPHICS =			0x6,
	VGA_GC_COLOR_DONT_CARE =		0x7,
	VGA_GC_BIT_MASK =			0x8
};

enum { /* CGA palette selection (320x200x4) */
	VGA_CGA_PALETTE_GR_BR_RD =		0,
	VGA_CGA_PALETTE_CY_MA_WH =		1
};

enum { /* color select (text=border color  320x200=background    640x200=foreground */
	VGA_CGA_PALETTE_CS_BLUE =		(1U << 0U),
	VGA_CGA_PALETTE_CS_GREEN =		(1U << 1U),
	VGA_CGA_PALETTE_CS_RED =		(1U << 2U),
	VGA_CGA_PALETTE_CS_INTENSITY =		(1U << 3U),
	VGA_CGA_PALETTE_CS_ALT_INTENSITY =	(1U << 4U)
};

extern uint32_t			vga_clock_rates[4];

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
void vga_write_state_DEBUG(FILE *f);
void vga_relocate_crtc(unsigned char color);
void vga_switch_to_mono();
void vga_switch_to_color();
void vga_turn_on_hgc();
void vga_turn_off_hgc();
void vga_enable_256color_modex();
uint16_t vga_get_start_location();
void vga_set_start_location(unsigned int offset);
void vga_splitscreen(unsigned int v);
void vga_alpha_switch_to_font_plane();
void vga_alpha_switch_from_font_plane();
void vga_set_9wide(unsigned char en);
void vga_select_charset_a_b(unsigned short a,unsigned short b);
void vga_write_crtc_mode(struct vga_mode_params *p,unsigned int flags);
void vga_correct_crtc_mode(struct vga_mode_params *p);
void vga_read_crtc_mode(struct vga_mode_params *p);

VGA_RAM_PTR get_pcjr_mem(void);

#define VGA_WRITE_CRTC_MODE_NO_CLEAR_SYNC	0x0001

static inline unsigned char vga_read_CRTC(unsigned char i) {
	outp(vga_state.vga_base_3x0+4,i);
	return inp(vga_state.vga_base_3x0+5);
}

static inline unsigned char vga_read_GC(unsigned char i) {
	outp(0x3CE,i);
	return inp(0x3CF);
}

static inline void vga_tandy_setpalette(unsigned char i,unsigned char c) {
    inp(0x3DA);
    if (vga_state.vga_flags & VGA_IS_PCJR) {
        outp(0x3DA,0x10 + i);
        outp(0x3DA,c);

        /* NTS: DOSBox machine=pcjr emulation seems to imply that writing palette
         *      registers blanks the display. Do dummy write to un-blank the display. */
        outp(0x3DA,0x00);
    }
    else {
        outp(0x3DA,0x10 + i);
        outp(0x3DE,c);	/* NTS: This works properly on Tandy [at least DOSBox] */
    }
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
	outp(vga_state.vga_base_3x0+4,i);
	outp(vga_state.vga_base_3x0+5,c);
}

static inline void vga_set_ypan_sub(unsigned char c) {
	vga_write_CRTC(0x08,c);
}

static inline void vga_write_color(unsigned char c) {
	vga_state.vga_color = c;
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

static inline void vga_set_cga_palette_and_background(unsigned char pal,unsigned char color) {
	outp(0x3D9,(pal << 5) | color);
}

static inline void vga_set_cga_mode(unsigned char b) {
	outp(0x3D8,b);
}

/* NTS: VGA hardware treats bit 5 of the index as a "screen enable".
 *      When the caller is done reprogramming it is expected to or the index by VGA_AC_ENABLE */
static inline void vga_write_AC(unsigned char i,unsigned char c) {
	inp(vga_state.vga_base_3x0+0xA); /* reset flipflop */
	outp(0x3C0,i);
	outp(0x3C0,c);
	inp(vga_state.vga_base_3x0+0xA);
}

static inline void vga_set_xpan(unsigned char c) {
	vga_write_AC(0x13 | VGA_AC_ENABLE,c);
}

static inline unsigned char vga_read_AC(unsigned char i) {
	unsigned char c;

	/* NTS: Reading the palette registers must occur this way because
	 *      an old S3 Virge DX card I have will misread the values
	 *      when PAS=1 otherwise. */

	inp(vga_state.vga_base_3x0+0xA);	/* reset latch */
	outp(0x3C0,i&(~0x20));			/* index with PAS=0 */
	c = inp(0x3C1);
	inp(vga_state.vga_base_3x0+0xA);	/* reset latch */
	outp(0x3C0,i|0x20);			/* index with PAS=1 */
	inp(vga_state.vga_base_3x0+0xA);	/* reset latch */

	return c;
}

static inline void vga_set_stride(unsigned int stride) {
	/* TODO: Knowing the current "byte/word/dword" memory size, compute properly */
	stride >>= (2+1); /* divide by DWORD * 2 */
	vga_write_CRTC(0x13,stride);
}

static inline void vga_AC_reenable_screen() {
	inp(vga_state.vga_base_3x0+0xA); /* reset flipflop */
	outp(0x3C0,0x20);
	inp(vga_state.vga_base_3x0+0xA);
}

static inline void vga_palette_lseek(unsigned int i) {
	outp(0x3C8,i);
}

static inline void vga_palette_write(unsigned char r,unsigned char g,unsigned char b) {
	outp(0x3C9,r);
	outp(0x3C9,g);
	outp(0x3C9,b);
}

void pal_buf_to_vga(unsigned int offset,unsigned int count,unsigned char *palette);

static unsigned int vga_rep_stosw(const unsigned char far * const vp,const uint16_t v,const unsigned int wc);
#pragma aux vga_rep_stosw = \
    "rep stosw" \
    parm [es di] [ax] [cx] \
    modify [di cx] \
    value [di];

static inline unsigned char vga_in_vsync() {
	return (inp(vga_state.vga_base_3x0 + 0xA) & 0x8);
}

static inline void vga_wait_for_hsync() {
	while ((inp(vga_state.vga_base_3x0 + 0xA) & 0x1) == 0);
}

static inline void vga_wait_for_hsync_end() {
	while ((inp(vga_state.vga_base_3x0 + 0xA) & 0x1) != 0);
}

static inline void vga_wait_for_vsync() {
	while ((inp(vga_state.vga_base_3x0 + 0xA) & 0x8) == 0);
}

static inline void vga_wait_for_vsync_end() {
	while ((inp(vga_state.vga_base_3x0 + 0xA) & 0x8) != 0);
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

void vga_wm1_mem_block_copy(uint16_t dst,uint16_t src,uint16_t b);
void vga_setup_wm1_block_copy();
void vga_restore_rm0wm0();

#ifdef __cplusplus
}
#endif

#endif //defined(TARGET_PC98)

#endif /* __DOSLIB_HW_VGA_VGA_H */

