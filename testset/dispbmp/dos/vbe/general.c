
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <dos.h>
#include <i86.h>

#include "libbmp.h"

static char *bmpfile = NULL;
static unsigned int img_width = 0;
static unsigned int img_height = 0;
static unsigned int img_stride = 0;
static unsigned int img_dac_width = 0;
static uint32_t window_size = 0,window_mult = 0;
static uint16_t window_bank_advance = 0;
static unsigned char force_vbe_palset = 0;
static unsigned char enable_8bit_dac = 1;
static unsigned char enable_rmwnfunc = 1;
static unsigned char enable_window = 1;
static unsigned char enable_lfb = 1;
static unsigned char pause_info = 0;

typedef void (*draw_scanline_bnksw_t)(uint16_t bank);
typedef void (*draw_scanline_func_t)(unsigned int y,unsigned char *src,unsigned int bytes);

static draw_scanline_bnksw_t draw_scanline_bank_switch;
static draw_scanline_func_t draw_scanline;

///

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

#if TARGET_MSDOS == 32
#pragma pack(push,1)
struct dpmi_realmode_call {
	uint32_t	edi,esi,ebp,reserved;
	uint32_t	ebx,edx,ecx,eax;
	uint16_t	flags,es,ds,fs,gs,ip,cs,sp,ss;
};
#pragma pack(pop)

static void vbe_realint(struct dpmi_realmode_call *rc) {
	__asm {
		mov	ax,0x0300
		mov	bx,0x0010
		xor	cx,cx
		mov	edi,rc		; we trust Watcom has left ES == DS
		int	0x31		; call DPMI
	}
}
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

#define FINDVBE_WINDOW			(1u << 0u)
#define FINDVBE_LFB			(1u << 1u)

static struct vbe_info_block vbe_info={0};
static struct vbe_mode_info vbe_modeinfo={0};
static uint16_t vbe_mode_number = 0;
static uint8_t vbe_mode_can_window = 0;
static uint8_t vbe_mode_can_lfb = 0;
static uint32_t vbe_mode_total_memory = 0;
static uint8_t vbe_mode_red_shift = 0;
static uint8_t vbe_mode_red_width = 0;
static uint8_t vbe_mode_green_shift = 0;
static uint8_t vbe_mode_green_width = 0;
static uint8_t vbe_mode_blue_shift = 0;
static uint8_t vbe_mode_blue_width = 0;
static uint8_t vbe_mode_res_shift = 0;
static uint8_t vbe_mode_res_width = 0;
#if TARGET_MSDOS == 32
static uint16_t vbe_dos_selector = 0; /* selector value returned by DPMI */
static void *vbe_dos_segment = NULL; /* pointer to DOS segment */
static void *lfb_lin_base = NULL;
#endif

#if TARGET_MSDOS == 32
static uint32_t real2lin(const uint32_t realfarptr) {
	const uint16_t segv = realfarptr >> 16u;
	const uint16_t ofsv = realfarptr & 0xFFFFu;
	return ((uint32_t)segv << (uint32_t)4u) + (uint32_t)ofsv;
}
#endif

#if TARGET_MSDOS == 32
void *dpmi_phys_addr_map(uint32_t phys,uint32_t size) {
	uint32_t retv = 0;

	__asm {
		mov	ax,0x0800
		mov	cx,word ptr phys
		mov	bx,word ptr phys+2
		mov	di,word ptr size
		mov	si,word ptr size+2
		int	0x31
		jc	endf
		mov	word ptr retv,cx
		mov	word ptr retv+2,bx
endf:
	}

	return (void*)retv;
}

void dpmi_phys_addr_free(void *base) {
	__asm {
		mov	ax,0x0801
		mov	cx,word ptr base
		mov	bx,word ptr base+2
		int	0x31
	}
}
#endif

#if TARGET_MSDOS == 32
void *dpmi_alloc_dos(unsigned long len,uint16_t *selector) {
	unsigned short rm=0,pm=0,fail=0;

	/* convert len to paragraphs */
	len = (len + 15) >> 4UL;
	if (len >= 0xFF00UL) return NULL;

	__asm {
		mov	bx,WORD PTR len
		mov	ax,0x100
		int	0x31

		mov	rm,ax
		mov	pm,dx
		sbb	ax,ax
		mov	fail,ax
	}

	if (fail) return NULL;

	*selector = pm;
	return (void*)((unsigned long)rm << 4UL);
}

void dpmi_free_dos(uint16_t selector) {
	__asm {
		mov	ax,0x101
		mov	dx,selector
		int	0x31
	}
}
#endif

static int detect_vbe(void) {
	uint16_t status = 0;

	/* please return extended info (512 bytes) when supported */
	memset(&vbe_info,0,sizeof(vbe_info));
	memcpy(&vbe_info.signature,"VBE2",4);

#if TARGET_MSDOS == 32
	if (vbe_dos_segment == NULL) {
		vbe_dos_segment = dpmi_alloc_dos(1024,&vbe_dos_selector);
		if (vbe_dos_segment == NULL)
			return 1;
	}

	{
		struct dpmi_realmode_call rc={0};
		rc.eax = 0x4F00;
		rc.edi = 0;
		rc.es = (uint16_t)((uint32_t)vbe_dos_segment >> 4ul);
		vbe_realint(&rc);
		status = (uint16_t)rc.eax;
		if (status == 0x004F) memcpy(&vbe_info,vbe_dos_segment,sizeof(vbe_info));
	}
#else
	__asm {
		push	es
		push	di
		mov	ax,0x4F00	; AH=4Fh AL=00h return SVGA information
		mov	di,seg vbe_info
		mov	es,di
		mov	di,offset vbe_info
		int	10h
		pop	di
		pop	es
		mov	status,ax
	}
#endif
	if (status != 0x004F) return 0;/* AH=00h successful AL=4Fh supported */
	if (memcmp(&vbe_info.signature,"VESA",4)) return 0; /* signature must be "VESA" */
	if (vbe_info.video_mode_ptr == 0ul || vbe_info.video_mode_ptr == 0xFFFFFFFFul) return 0; /* no video modes... riiiiight */
	return 1;
}

static int accept_can_lfb_mode(unsigned int flags) {
	(void)flags;

	if (!(vbe_modeinfo.mode_attributes & VESA_MODE_ATTR_LINEAR_FRAMEBUFFER_AVAILABLE))
		return 0;
	if (vbe_modeinfo.phys_base_ptr == 0)
		return 0;

	return 1;
}

static int accept_can_window_mode(unsigned int flags) {
	(void)flags;

	/* windowing must be available */
	if (vbe_modeinfo.mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE_WINDOWING)
		return 0;

	/* bank switching validation */
	if (vbe_modeinfo.win_granularity == 0 || vbe_modeinfo.win_granularity > 128)
		return 0;
	if (vbe_modeinfo.win_size == 0 || vbe_modeinfo.win_size > 128)
		return 0;
	/* window A must be valid, writeable */
	/* TODO: In case of weird BIOSes, auto select window B if window A is not writeable */
	if ((vbe_modeinfo.win_a_attributes & 5) != 5) /* bit2=window writeable bit0=window supported */
		return 0;
	if (vbe_modeinfo.win_a_segment == 0)
		return 0;

	return 1;
}

static int accept_mode(unsigned int flags,unsigned int width,unsigned int height,unsigned int bpp) {
	vbe_mode_can_window = 0;
	vbe_mode_can_lfb = 0;

	/* must be supported, color, graphics */
	if ((vbe_modeinfo.mode_attributes & (VESA_MODE_ATTR_HW_SUPPORTED|VESA_MODE_ATTR_GRAPHICS_MODE|VESA_MODE_ATTR_COLOR_MODE)) !=
		(VESA_MODE_ATTR_HW_SUPPORTED|VESA_MODE_ATTR_GRAPHICS_MODE|VESA_MODE_ATTR_COLOR_MODE))
		return 0;

	/* believe it or not, VESA BIOS 1.0 standard allowed the BIOS to return information without any resolution information */
	if ((vbe_modeinfo.mode_attributes & VESA_MODE_ATTR_VBE_1X_MOREINFO) == 0)
		return 0; /* not supported yet */

	/* additional validation */
	if (vbe_modeinfo.bytes_per_scan_line == 0)
		return 0;

	if (flags & FINDVBE_WINDOW) {
		if (accept_can_window_mode(flags))
			vbe_mode_can_window = 1;
	}

	if (flags & FINDVBE_LFB) {
		if (accept_can_lfb_mode(flags))
			vbe_mode_can_lfb = 1;
	}

	/* if it can't do either than do not use this mode */
	if (!vbe_mode_can_window && !vbe_mode_can_lfb)
		return 0;

	/* OK, match video resolution */
	if (vbe_modeinfo.x_resolution == width && vbe_modeinfo.y_resolution == height) {
		if (vbe_modeinfo.bits_per_pixel == bpp) {
			if (vbe_modeinfo.bits_per_pixel == 8 && vbe_modeinfo.number_of_planes <= 1 &&
				(vbe_modeinfo.memory_model == 0x04/*packed*/ || vbe_modeinfo.memory_model == 0x05/*non-chain 256-color*/)) {
				return 1;
			}
			else if ((vbe_modeinfo.bits_per_pixel == 24 || vbe_modeinfo.bits_per_pixel == 32 ||
				vbe_modeinfo.bits_per_pixel == 15 || vbe_modeinfo.bits_per_pixel == 16)
				&& vbe_modeinfo.number_of_planes <= 1 &&
				(vbe_modeinfo.memory_model == 0x04/*packed*/ || vbe_modeinfo.memory_model == 0x06/*direct color*/)) {

				if (vbe_modeinfo.memory_model == 0x06) {
					/* if the caller asked for 15bpp then match 5:5:5
					 * if the caller asked for 16bpp then match 5:6:5 */
					if (bpp == 15) {
						if (vbe_modeinfo.blue_mask_size != 5 || vbe_modeinfo.green_mask_size != 5 || vbe_modeinfo.red_mask_size != 5)
							return 0;
					}
					else if (bpp == 16) {
						if (vbe_modeinfo.blue_mask_size != 5 || vbe_modeinfo.green_mask_size != 6 || vbe_modeinfo.red_mask_size != 5)
							return 0;
					}

					vbe_mode_blue_shift = vbe_modeinfo.blue_field_position;
					vbe_mode_blue_width = vbe_modeinfo.blue_mask_size;
					vbe_mode_green_shift = vbe_modeinfo.green_field_position;
					vbe_mode_green_width = vbe_modeinfo.green_mask_size;
					vbe_mode_red_shift = vbe_modeinfo.red_field_position;
					vbe_mode_red_width = vbe_modeinfo.red_mask_size;
					vbe_mode_res_shift = vbe_modeinfo.reserved_field_position;
					vbe_mode_res_width = vbe_modeinfo.reserved_mask_size;
				}
				else {
					// TODO: VESA BIOS 1.1 and earlier where directcolor was packed and you assumed formats.
					return 0;
				}

				return 1;
			}
		}
	}

	return 0;
}

static unsigned int find_vbe_mode(unsigned int flags,unsigned int width,unsigned int height,unsigned int bpp) {
	unsigned int ret = 0;
	unsigned int count = 0;
	uint16_t status = 0;

#if TARGET_MSDOS == 32
	uint16_t *modearray;
#else
	uint16_t far *modearray;
#endif

	/* search mode list */
#if TARGET_MSDOS == 32
	modearray = (uint16_t*)real2lin(vbe_info.video_mode_ptr);
#else
	modearray = (uint16_t far*)vbe_info.video_mode_ptr;
#endif

	while (*modearray != 0xFFFF && count < 4096) {
		uint16_t mode = *modearray;

#if TARGET_MSDOS == 32
		if (vbe_dos_segment != NULL) {
			struct dpmi_realmode_call rc={0};
			rc.eax = 0x4F01;
			rc.ecx = mode;
			rc.edi = 0;
			rc.es = (uint16_t)((uint32_t)vbe_dos_segment >> 4ul);
			vbe_realint(&rc);
			status = (uint16_t)rc.eax;
			if (status == 0x004F) memcpy(&vbe_modeinfo,vbe_dos_segment,sizeof(vbe_modeinfo));
		}
#else
		__asm {
			push		es
			push		di
			mov		ax,0x4F01	; AH=4Fh AL=01h get mode information
			mov		cx,mode
			mov		di,seg vbe_modeinfo
			mov		es,di
			mov		di,offset vbe_modeinfo
			int		10h
			pop		di
			pop		es
			mov		status,ax
		}
#endif
		if (status == 0x004F) { /* AH=00h successful AL=4Fh supported */
			if (accept_mode(flags,width,height,bpp)) {
				vbe_mode_number = ret = mode;

				window_size = (uint32_t)vbe_modeinfo.win_size << 10ul;
				window_mult = (uint32_t)vbe_modeinfo.win_granularity << 10ul;
				window_bank_advance = (uint16_t)(window_size / window_mult);
				if (window_bank_advance == 0) window_bank_advance = 1;

				break;
			}
		}
		
		modearray++;
		count++;
	}

	vbe_mode_total_memory = (uint32_t)vbe_info.total_memory_64kb << (uint32_t)16ul;
	return ret;
}

static int set_vbe_mode(uint16_t mode) {
	uint16_t status = 0;

	(void)mode;

	__asm {
		mov	ax,0x4F02	; AH=4Fh AL=02h set video mode
		mov	bx,mode
		int	10h
		mov	status,ax
	}

	if (status != 0x004F) return 0; /* AH=0x00 success AL=0x4F supported */

	return 1;
}

static int vbe_set_dac_width(uint8_t w) {
	uint16_t status = 0;

	(void)w;

	__asm {
		mov	ax,0x4F08	; AH=4Fh AL=08h DAC palette control
		mov	bl,0		; set it
		mov	bh,w
		int	10h
		mov	status,ax
	}

	if (status != 0x004F) return 0; /* AH=0x00 success AL=0x4F supported */

	return 1;
}

static void vbe_set_palette(unsigned int first,unsigned int count,unsigned char *pal) {
	(void)first;
	(void)count;
	(void)pal;

	if (first >= 256) return;
	if (count > 256) return;
	if ((first+count) > 256) return;

#if TARGET_MSDOS == 32
	if (vbe_dos_segment != NULL) {
		struct dpmi_realmode_call rc={0};
		memcpy(vbe_dos_segment,pal,4*count);
		rc.eax = 0x4F09;
		rc.ecx = count;
		rc.edx = first;
		rc.edi = 0;
		rc.es = (uint16_t)((uint32_t)vbe_dos_segment >> 4ul);
		vbe_realint(&rc);
	}
#else
	__asm {
		push	es
		push	di
		mov	ax,0x4F09	; AH=4Fh AL=09h palette data
		mov	bl,0x00		; set palette
		mov	cx,count
		mov	dx,first
#if defined(__LARGE__) || defined(__COMPACT__) || defined(__HUGE__)
		les	di,dword ptr [pal]
#else
		push	ds
		pop	es
		mov	di,[pal]
#endif
		int	10h
		pop	di
		pop	es
	}
#endif
}

static void vbe_bank_switch_int10(uint16_t bank) {
	(void)bank;

	__asm {
		mov	ax,0x4F05	; AH=4Fh AL=05h CPU memory window control
		mov	bx,0		; Set window, Window A
		mov	dx,bank		; window position in granularity units
		int	10h
	}
}

#if TARGET_MSDOS == 16 /* 16-bit only. You could call from 32-bit protected mode but it's harder to do. */
static void vbe_bank_switch_rmwnfunc(uint16_t bank) {
	uint32_t proc = vbe_modeinfo.window_function;
	if (proc == 0) return;

	__asm {
		mov	ax,0x4F05	; AH=4Fh AL=05h CPU memory window control
		mov	bx,0		; Set window, Window A
		mov	dx,bank		; window position in granularity units
		call	dword ptr [proc]
	}
}
#endif

///

static uint16_t current_bank = ~0u;

static void bnksw_int10(uint16_t bank) {
	vbe_bank_switch_int10(bank);
}

#if TARGET_MSDOS == 16 /* 16-bit only. You could call from 32-bit protected mode but it's harder to do. */
static void bnksw_rmwnfnc(uint16_t bank) {
	vbe_bank_switch_rmwnfunc(bank);
}
#endif

#if TARGET_MSDOS == 32
static void draw_scanline_lfb(unsigned int y,unsigned char *src,unsigned int bytes) {
	if (y < img_height && lfb_lin_base) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		unsigned char *d = (unsigned char*)lfb_lin_base + addr;
		memcpy(d,src,bytes);
	}
}
#endif

static void draw_scanline_bnksw(unsigned int y,unsigned char *src,unsigned int bytes) {
	if (y < img_height) {
		const uint32_t addr = ((uint32_t)y * (uint32_t)img_stride);
		uint16_t bank = (uint16_t)(addr / window_mult);
		uint32_t bnkaddr = (addr % window_mult);
#if TARGET_MSDOS == 32
		unsigned char *d = (unsigned char*)((uint32_t)vbe_modeinfo.win_a_segment << 4ul) + bnkaddr;
#else
		unsigned char far *d = MK_FP(vbe_modeinfo.win_a_segment,bnkaddr);
#endif
		unsigned int cpy;

		if (current_bank != bank) {
			draw_scanline_bank_switch(bank);
			current_bank = bank;
		}

		if (bnkaddr != 0) {
			cpy = window_size - bnkaddr;
			if (cpy > bytes) cpy = bytes;
		}
		else {
			cpy = bytes;
		}

#if TARGET_MSDOS == 32
		memcpy(d,src,cpy);
#else
		_fmemcpy(d,src,cpy);
#endif
		bytes -= cpy;
		if (bytes != 0) {
			src += cpy;
			bank += window_bank_advance;

			if (current_bank != bank) {
				draw_scanline_bank_switch(bank);
				current_bank = bank;
			}

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA0000;
			memcpy(d,src,bytes);
#else
			d = MK_FP(0xA000,0);
			_fmemcpy(d,src,bytes);
#endif
		}
	}
}

static void help(void) {
	fprintf(stderr,"general [opts] <bmp file>\n");
	fprintf(stderr," -no-8bit-dac          Do not attempt 8-bit DAC\n");
	fprintf(stderr," -no-rmwnfunc          Do not use real-mode direct call window control\n");
	fprintf(stderr," -no-lfb               Do not use linear framebuffer\n");
	fprintf(stderr," -no-window            Do not use bank switching window\n");
	fprintf(stderr," -pause-info           Pause to show info before setting mode\n");
	fprintf(stderr," -vbe-palset           Always use VBE functions to set palette\n");
}

static int parse_argv(int argc,char **argv) {
	int nsw = 0;
	int i = 1;
	char *a;

	while (i < argc) {
		a = argv[i++];

		if (*a == '-') {
			do { a++; } while (*a == '-');

			if (!strcmp(a,"?") || !strcmp(a,"h") || !strcmp(a,"help")) {
				help();
				return 0;
			}
			else if (!strcmp(a,"no-8bit-dac")) {
				enable_8bit_dac = 0;
			}
			else if (!strcmp(a,"no-rmwnfunc")) {
				enable_rmwnfunc = 0;
			}
			else if (!strcmp(a,"no-lfb")) {
				enable_lfb = 0;
			}
			else if (!strcmp(a,"no-window")) {
				enable_window = 0;
			}
			else if (!strcmp(a,"pause-info")) {
				pause_info = 1;
			}
			else if (!strcmp(a,"vbe-palset")) {
				force_vbe_palset = 1;
			}
			else {
				fprintf(stderr,"Unknown switch %s\n",a);
				return 0;
			}
		}
		else {
			switch (nsw) {
				case 0:
					bmpfile = a;
					break;
				case 1:
					fprintf(stderr,"Excess param\n");
					return 0;
			};

			nsw++;
		}
	}

	if (bmpfile == NULL) {
		fprintf(stderr,"BMP file not specified\n");
		return 0;
	}

	return 1;
}

typedef void (*conv_scanline_func_t)(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes);
static conv_scanline_func_t convert_scanline;

void convert_scanline_none(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes) {
	(void)bytes;
	(void)src;
	(void)bfr;
}

void convert_scanline_32bpp8(struct BMPFILEREAD *bfr,unsigned char *src,unsigned int bytes) {
	uint32_t *s32 = (uint32_t*)src;

	bytes >>= 2u;
	while (bytes-- > 0u) {
		uint32_t f;

		f  = ((*s32 >> (uint32_t)bfr->red_shift) & 0xFFu) << (uint32_t)vbe_mode_red_shift;
		f += ((*s32 >> (uint32_t)bfr->green_shift) & 0xFFu) << (uint32_t)vbe_mode_green_shift;
		f += ((*s32 >> (uint32_t)bfr->blue_shift) & 0xFFu) << (uint32_t)vbe_mode_blue_shift;
		*s32++ = f;
	}
}

int main(int argc,char **argv) {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;
	uint16_t vbemode = 0;

	if (!parse_argv(argc,argv))
		return 1;

#if TARGET_MSDOS == 16
	/* we cannot support the linear framebuffer.
	 * or, well, we COULD, but that requires some extra trickery to
	 * enable "flat real mode" to reach up into extended memory to
	 * touch the framebuffer. to help keep this code simple, we do
	 * not support real-mode linear framebuffer access at this time. */
	if (enable_lfb) {
		fprintf(stderr,"Real-mode builds do not support the LFB at this time, sorry.\n");
		enable_lfb = 0;
	}
#endif

	if (!detect_vbe()) {
		fprintf(stderr,"VESA BIOS extensios not detected\n");
		return 1;
	}

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp == 0 || bfr->bpp > 32) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}
	if (bfr->bpp <= 8 && (bfr->colors == 0 || bfr->palette == NULL)) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* VBE set palette call didn't exist until VBE 2.0 */
	if (force_vbe_palset && vbe_info.version < 0x200) {
		fprintf(stderr,"Cannot force VBE call to set palette because VESA BIOS follows pre-2.0 standard\n");
		force_vbe_palset = 0;
	}

	/* TODO: This is where it might be handy to support the Windows 95 BI_BITFIELDS extension to distinguish 15/16bpp */
	fprintf(stderr,"BMP is %u x %u x %ubpp\n",bfr->width,bfr->height,bfr->bpp);
	if (bfr->colors != 0) fprintf(stderr,"With a %u color palette\n",bfr->colors);
	fprintf(stderr,"BMP R/G/B/%c{shift,width}: {%u,%u} {%u,%u} {%u,%u} {%u,%u}\n",
		bfr->has_alpha?'A':'X',
		bfr->red_shift,bfr->red_width,
		bfr->green_shift,bfr->green_width,
		bfr->blue_shift,bfr->blue_width,
		bfr->alpha_shift,bfr->alpha_width);

	/* Now find a VESA BIOS mode that matches the bitmap */
	{
		unsigned int mode_flags = 0;

		if (enable_window) mode_flags |= FINDVBE_WINDOW;
		if (enable_lfb) mode_flags |= FINDVBE_LFB;

		if (bfr->bpp == 16 && bfr->green_width == 5 && bfr->red_width == 5 && bfr->blue_width == 5) /* 5:5:5 */
			vbemode = find_vbe_mode(mode_flags,bfr->width,bfr->height,15);
		else
			vbemode = find_vbe_mode(mode_flags,bfr->width,bfr->height,bfr->bpp);
	}
	if (vbemode == 0) {
		fprintf(stderr,"No matching VESA BIOS mode\n");
		return 1;
	}

	convert_scanline = convert_scanline_none;

	img_width = vbe_modeinfo.x_resolution;
	img_height = vbe_modeinfo.y_resolution;
	img_stride = vbe_modeinfo.bytes_per_scan_line;

	fprintf(stderr,"Found matching VESA BIOS mode 0x%x\n",vbemode);

	if (window_size == 0 || window_mult == 0 || window_bank_advance == 0) {
		fprintf(stderr,"ERROR: Bad window values\n");
		return 1;
	}

	fprintf(stderr,"VBE board has %lu (0x%lx) bytes of memory\n",(unsigned long)vbe_mode_total_memory,(unsigned long)vbe_mode_total_memory);
	fprintf(stderr,"Bank switching: %s (window at 0x%04x0)\n",vbe_mode_can_window?"yes":"no",vbe_modeinfo.win_a_segment);
	fprintf(stderr,"Linear framebuffer: %s (lfb at 0x%08lx)\n",vbe_mode_can_lfb?"yes":"no",(unsigned long)vbe_modeinfo.phys_base_ptr);
	fprintf(stderr,"R/G/B/X{shift,width}: {%u,%u} {%u,%u} {%u,%u} {%u,%u}\n",
		vbe_mode_red_shift,vbe_mode_red_width,
		vbe_mode_green_shift,vbe_mode_green_width,
		vbe_mode_blue_shift,vbe_mode_blue_width,
		vbe_mode_res_shift,vbe_mode_res_width);

	if (vbe_mode_can_lfb) {
#if TARGET_MSDOS == 32 /* 32-bit only. For 16-bit real mode to touch the LFB would take flat real mode or other tricks. */
		draw_scanline_bank_switch = NULL;
		draw_scanline = draw_scanline_lfb;
		fprintf(stderr,"I will draw using linear framebuffer\n");
#else
		fprintf(stderr,"How did LFB enable occur anyway??\n");
		return 1;
#endif
	}
	else if (vbe_mode_can_window) {
		draw_scanline_bank_switch = bnksw_int10;
#if TARGET_MSDOS == 16 /* 16-bit only. You could call from 32-bit protected mode but it's harder to do. */
		if (vbe_modeinfo.window_function != 0 && enable_rmwnfunc) draw_scanline_bank_switch = bnksw_rmwnfnc;
#endif
		draw_scanline = draw_scanline_bnksw;
		fprintf(stderr,"I will draw using bank switching\n");
	}
	else {
		fprintf(stderr,"No method to draw?\n");
		return 1;
	}

	if (bfr->bpp == 32) {
		if (	vbe_mode_red_shift != bfr->red_shift ||
			vbe_mode_green_shift != bfr->green_shift ||
			vbe_mode_blue_shift != bfr->blue_shift) {
			if (vbe_mode_red_width == bfr->red_width && vbe_mode_green_width == bfr->green_width && vbe_mode_blue_width == bfr->blue_width) {
				if (bfr->red_width == 8 && bfr->green_width == 8 && bfr->blue_width == 8) {
					convert_scanline = convert_scanline_32bpp8;
				}
			}
		}
	}

	fprintf(stderr,"Final VBE mode 0x%x\n",vbemode);

	if (pause_info) {
		fprintf(stderr,"Hit enter to continue\n");
		while (getch() != 13);
	}

	if (!set_vbe_mode(vbemode)) {
		fprintf(stderr,"Unable to set mode\n");
		return 1;
	}

#if TARGET_MSDOS == 32 /* 32-bit only. For 16-bit real mode to touch the LFB would take flat real mode or other tricks. */
	if (vbe_mode_can_lfb) {
		if ((vbe_modeinfo.bytes_per_scan_line * vbe_modeinfo.y_resolution) > vbe_mode_total_memory) {
			__asm {
				mov	ax,0x0003	; AH=0x00 AL=0x03
				int	0x10
			}
			fprintf(stderr,"Not enough video RAM\n");
			return 1;
		}

		lfb_lin_base = dpmi_phys_addr_map(vbe_modeinfo.phys_base_ptr,vbe_modeinfo.bytes_per_scan_line * vbe_modeinfo.y_resolution);
		if (lfb_lin_base == NULL) {
			__asm {
				mov	ax,0x0003	; AH=0x00 AL=0x03
				int	0x10
			}
			fprintf(stderr,"Cannot map linear framebuffer\n");
			return 1;
		}
	}
#endif

	if ((vbe_info.capabilities & VBE_CAP_8BIT_DAC) && enable_8bit_dac) {
		if (vbe_set_dac_width(8)) {
			img_dac_width = 8;
		}
	}

	/* set palette */
	if (vbe_modeinfo.bits_per_pixel == 8) {
		unsigned int shf = 2;

		if (img_dac_width == 8) shf = 0;

		if ((vbe_modeinfo.mode_attributes & VESA_MODE_ATTR_NOT_VGA_COMPATIBLE) || force_vbe_palset) {
			unsigned char *pal = malloc(256*4);
			if (pal) {
				for (i=0;i < bfr->colors;i++) {
					pal[i*4+0] = bfr->palette[i].rgbBlue >> shf;
					pal[i*4+1] = bfr->palette[i].rgbGreen >> shf;
					pal[i*4+2] = bfr->palette[i].rgbRed >> shf;
					pal[i*4+3] = 0;
				}
				vbe_set_palette(0,256,pal);
				free(pal);
			}
		}
		else {
			outp(0x3C8,0);
			for (i=0;i < bfr->colors;i++) {
				outp(0x3C9,bfr->palette[i].rgbRed >> shf);
				outp(0x3C9,bfr->palette[i].rgbGreen >> shf);
				outp(0x3C9,bfr->palette[i].rgbBlue >> shf);
			}
		}
	}

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	dispw = ((dispw * bfr->bpp) + 7u) >> 3u;
	while (read_bmp_line(bfr) == 0) {
		convert_scanline(bfr,bfr->scanline,dispw);
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);
	}

	/* done reading */
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

#if TARGET_MSDOS == 32 /* 32-bit only. For 16-bit real mode to touch the LFB would take flat real mode or other tricks. */
	if (vbe_mode_can_lfb && lfb_lin_base) {
		dpmi_phys_addr_free(lfb_lin_base);
		lfb_lin_base = NULL;
	}
#endif

	/* restore text */
	__asm {
		mov	ax,0x0003	; AH=0x00 AL=0x03
		int	0x10
	}
	return 0;
}

