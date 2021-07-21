
#include <hw/cpu/cpu.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DAC can do 8-bit color */
#define VBE_CAP_8BIT_DAC		(1 << 0)
/* controller is NOT VGA compatible */
#define VBE_CAP_NOT_VGA_COMPATIBLE	(1 << 1)
/* RAMDAC recommended blanking during large data transfers (in other words: an older SVGA controller that is single ported and can cause "snow" on screen if RAM is written during active display----same problem as the IBM Color Graphics Adapter) */
#define VBE_CAP_LARGE_XFER_BLANKING	(1 << 2)

#define VESA_MODE_ATTR_HW_SUPPORTED			(1 << 0)
#define VESA_MODE_ATTR_VBE_1X_MOREINFO			(1 << 1)
#define VESA_MODE_ATTR_BIOS_TTY				(1 << 2)
#define VESA_MODE_ATTR_COLOR_MODE			(1 << 3)
#define VESA_MODE_ATTR_GRAPHICS_MODE			(1 << 4)
#define VESA_MODE_ATTR_NOT_VGA_COMPATIBLE		(1 << 5)
#define VESA_MODE_ATTR_NOT_VGA_COMPATIBLE_WINDOWING	(1 << 6)
#define VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE	(1 << 7)
#define VESA_MODE_ATTR_DOUBLESCAN_AVAILABLE		(1 << 8)
#define VESA_MODE_ATTR_INTERLACED_AVAILABLE		(1 << 9)
#define VESA_MODE_ATTR_HARDWARE_TRIPLE_BUFFER_SUPPORT	(1 << 10)
#define VESA_MODE_ATTR_HARDWARE_STEREO_SUPPORT		(1 << 11)
#define VESA_MODE_ATTR_DUAL_DISPLAY_START_SUPPORT	(1 << 12)

#if TARGET_MSDOS == 32
# define VBE_DOS_BUFFER_SIZE	4096
#endif

#pragma pack(push,1)
struct vbe_info_block { /* for 1.x (256-byte) or 2.x/3.x (512-byte) */
	uint32_t	signature;		/* +0x000 'VESA' */
	uint16_t	version;		/* +0x004 */
	uint32_t	oem_string_ptr;		/* +0x006 */
	uint32_t	capabilities;		/* +0x00A */
	uint32_t	video_mode_ptr;		/* +0x00E */
	uint16_t	total_memory_64kb;	/* +0x012 */
	uint16_t	oem_software_rev;	/* +0x014 */
	uint32_t	oem_vendor_name_ptr;	/* +0x016 */
	uint32_t	oem_product_name_ptr;	/* +0x01A */
	uint32_t	oem_product_rev_ptr;	/* +0x01E */
	uint8_t		scratch[222];		/* +0x022 */
	/* VBE 2.x */
	uint8_t		oem_data[256];		/* +0x100 */
};						/* +0x200 */

struct vbe_mode_info {
	uint16_t	mode_attributes;	/* +0x000 */
	uint8_t		win_a_attributes;	/* +0x002 */
	uint8_t		win_b_attributes;	/* +0x003 */
	uint16_t	win_granularity;	/* +0x004 */
	uint16_t	win_size;		/* +0x006 */
	uint16_t	win_a_segment;		/* +0x008 */
	uint16_t	win_b_segment;		/* +0x00A */
	uint32_t	window_function;	/* +0x00C */
	uint16_t	bytes_per_scan_line;	/* +0x010 */
	/* VBE 1.2 */				/* =0x012 */
	uint16_t	x_resolution;		/* +0x012 */
	uint16_t	y_resolution;		/* +0x014 */
	uint8_t		x_char_size;		/* +0x016 */
	uint8_t		y_char_size;		/* +0x017 */
	uint8_t		number_of_planes;	/* +0x018 */
	uint8_t		bits_per_pixel;		/* +0x019 */
	uint8_t		number_of_banks;	/* +0x01A */
	uint8_t		memory_model;		/* +0x01B */
	uint8_t		bank_size;		/* +0x01C */
	uint8_t		number_of_image_pages;	/* +0x01D */
	uint8_t		reserved_1;		/* +0x01E */
	/* direct color fields */		/* =0x01F */
	uint8_t		red_mask_size;		/* +0x01F */
	uint8_t		red_field_position;	/* +0x020 */
	uint8_t		green_mask_size;	/* +0x021 */
	uint8_t		green_field_position;	/* +0x022 */
	uint8_t		blue_mask_size;		/* +0x023 */
	uint8_t		blue_field_position;	/* +0x024 */
	uint8_t		reserved_mask_size;	/* +0x025 */
	uint8_t		reserved_field_position;/* +0x026 */
	uint8_t		direct_color_mode_info;	/* +0x027 */
	/* VBE 2.0 */				/* =0x028 */
	uint32_t	phys_base_ptr;		/* +0x028 */
	uint32_t	reserved_2;		/* +0x02C */
	uint16_t	reserved_3;		/* +0x030 */
	/* VBE 3.0 */
	uint16_t	lin_bytes_per_line;	/* +0x032 */
	uint8_t		bank_number_of_image_pages;/*+0x034 */
	uint8_t		lin_number_of_image_pages;/*+0x035 */
	uint8_t		lin_red_mask_size;	/* +0x036 */
	uint8_t		lin_red_field_position;	/* +0x037 */
	uint8_t		lin_green_mask_size;	/* +0x038 */
	uint8_t		lin_green_field_position;/* +0x039 */
	uint8_t		lin_blue_mask_size;	/* +0x03A */
	uint8_t		lin_blue_field_position;/* +0x03B */
	uint8_t		lin_reserved_mask_size;	/* +0x03C */
	uint8_t		lin_reserved_field_position;/* +0x03D */
	uint32_t	max_pixel_clock;	/* +0x03E */
	uint8_t		reserved[190];		/* +0x042 */
};						/* =0x100 */

struct vbe_mode_custom_crtc_info {
	uint16_t	horizontal_total;
	uint16_t	horizontal_sync_start;
	uint16_t	horizontal_sync_end;
	uint16_t	vertical_total;
	uint16_t	vertical_sync_start;
	uint16_t	vertical_sync_end;
	uint8_t		flags;
	uint32_t	pixel_clock;
	uint16_t	refresh_rate;
	uint8_t		reserved[45];
};

struct vbe_mode_decision {
	int		mode;
	signed char	dac8;
	signed char	lfb;
	unsigned char	no_wf:1;
	unsigned char	force_wf:1;
	unsigned char	_pad_:6;
};
#pragma pack(pop)

extern struct vbe_info_block*		vbe_info;

void vbe_free();
int vbe_probe();
const char *vbe_memory_model_to_str(uint8_t m);
void vbe_realint(struct dpmi_realmode_call *rc);
void vbe_realbnk(struct dpmi_realmode_call *rc);
int vbe_read_mode_info(uint16_t mode,struct vbe_mode_info *mi);
void vbe_bank_switch(uint32_t rproc,uint8_t window,uint16_t bank);
int vbe_fill_in_mode_info(uint16_t mode,struct vbe_mode_info *mi);
uint16_t vbe_read_mode_entry(uint32_t mode_ptr,unsigned int entry);
int vbe_set_mode(uint16_t mode,struct vbe_mode_custom_crtc_info *ci);
void vesa_set_palette_data(unsigned int first,unsigned int count,char *pal);
void vbe_copystring(char *s,size_t max,uint32_t rp,struct vbe_info_block *b);
int vbe_set_dac_width(int w);
void vbe_reset_video_to_text();
void vbe_mode_decision_init(struct vbe_mode_decision *m);
int vbe_mode_decision_setmode(struct vbe_mode_decision *md,struct vbe_mode_info *mi);
int vbe_mode_decision_validate(struct vbe_mode_decision *md,struct vbe_mode_info *mi);
int vbe_mode_decision_acceptmode(struct vbe_mode_decision *md,struct vbe_mode_info *mi);

#define VBE_MODE_CUSTOM_REFRESH		(1 << 11)
#define VBE_MODE_LINEAR			(1 << 14)
#define VBE_MODE_DONT_CLEAR_RAM		(1 << 15)

extern uint32_t			vesa_lfb_base;
extern uint32_t			vesa_bnk_rproc;
extern uint8_t			vesa_bnk_window;		/* which window to use */
extern uint16_t			vesa_bnk_winseg;
extern uint16_t			vesa_bnk_winshf;
extern uint16_t			vesa_bnk_winszshf;
extern uint16_t			vesa_bnk_wincur;

#if TARGET_MSDOS == 32
extern void*			vesa_lfb_dpmi_map;
extern size_t			vesa_lfb_dpmi_map_size;
extern uint8_t			vesa_lfb_dpmi_pam;		/* set to 1: used DPMI function 0x0800 to obtain the pointer */
/* TODO: what if we have to use the "alloc/map device linear pages" 1.0 API */
#endif

void vesa_lfb_writeb(uint32_t ofs,uint8_t b);
void vesa_lfb_writew(uint32_t ofs,uint16_t b);
void vesa_lfb_writed(uint32_t ofs,uint32_t b);
void vesa_bnk_writeb(uint32_t ofs,uint8_t b);
void vesa_bnk_writew(uint32_t ofs,uint16_t b);
void vesa_bnk_writed(uint32_t ofs,uint32_t b);

extern void (*vesa_writeb)(uint32_t ofs,uint8_t b);
extern void (*vesa_writew)(uint32_t ofs,uint16_t b);
extern void (*vesa_writed)(uint32_t ofs,uint32_t b);

#ifdef __cplusplus
}
#endif

