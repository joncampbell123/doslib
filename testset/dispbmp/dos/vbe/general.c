
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <conio.h>
#include <stdio.h>
#include <errno.h>
#include <i86.h>

#include "libbmp.h"

static char *bmpfile = NULL;
static unsigned int img_width = 0;
static unsigned int img_height = 0;
static unsigned int img_stride = 0;
static unsigned int img_dac_width = 0;
static uint32_t window_size = 0,window_mult = 0;
static uint16_t window_bank_advance = 0;

typedef void (*draw_scanline_bnksw_t)(uint16_t bank);
typedef void (*draw_scanline_func_t)(unsigned int y,unsigned char *src,unsigned int pixels);

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

static struct vbe_info_block vbe_info={0};
static struct vbe_mode_info vbe_modeinfo={0};
static uint16_t vbe_mode_number = 0;

#if TARGET_MSDOS == 32
static uint32_t real2lin(const uint32_t realfarptr) {
	const uint16_t segv = realfarptr >> 16u;
	const uint16_t ofsv = realfarptr & 0xFFFFu;
	return ((uint32_t)segv << (uint32_t)4u) + (uint32_t)ofsv;
}
#endif

static int detect_vbe(void) {
	uint16_t status = 0;

	/* please return extended info (512 bytes) when supported */
	memset(&vbe_info,0,sizeof(vbe_info));
	memcpy(&vbe_info.signature,"VBE2",4);

#if TARGET_MSDOS == 32
	return 0;//TODO
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

static int accept_mode(unsigned int width,unsigned int height,unsigned int bpp) {
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

	/* OK, match video resolution */
	if (vbe_modeinfo.x_resolution == width && vbe_modeinfo.y_resolution == height) {
		if (vbe_modeinfo.bits_per_pixel == bpp) {
			if (vbe_modeinfo.bits_per_pixel == 8 && vbe_modeinfo.number_of_planes <= 1 &&
				(vbe_modeinfo.memory_model == 0x04/*packed*/ || vbe_modeinfo.memory_model == 0x05/*non-chain 256-color*/)) {
				return 1;
			}
		}
	}

	return 0;
}

static unsigned int find_vbe_mode(unsigned int width,unsigned int height,unsigned int bpp) {
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
		// TODO
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
			if (accept_mode(width,height,bpp)) {
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

	return ret;
}

static int set_vbe_mode(uint16_t mode) {
	uint16_t status = 0;

	(void)mode;

#if TARGET_MSDOS == 32
	return 0;//TODO
#else
	__asm {
		mov	ax,0x4F02	; AH=4Fh AL=02h set video mode
		mov	bx,mode
		int	10h
		mov	status,ax
	}
#endif

	if (status != 0x004F) return 0; /* AH=0x00 success AL=0x4F supported */

	return 1;
}

static int vbe_set_dac_width(uint8_t w) {
	uint16_t status = 0;

	(void)w;

#if TARGET_MSDOS == 32
	return 0;//TODO
#else
	__asm {
		mov	ax,0x4F08	; AH=4Fh AL=08h DAC palette control
		mov	bl,0		; set it
		mov	bh,w
		int	10h
		mov	status,ax
	}
#endif

	if (status != 0x004F) return 0; /* AH=0x00 success AL=0x4F supported */

	return 1;
}

static void vbe_bank_switch_int10(uint16_t bank) {
	(void)bank;

#if TARGET_MSDOS == 32
	// TODO
#else
	__asm {
		mov	ax,0x4F05	; AH=4Fh AL=05h CPU memory window control
		mov	bx,0		; Set window, Window A
		mov	dx,bank		; window position in granularity units
		int	10h
	}
#endif
}

static void vbe_bank_switch_rmwnfunc(uint16_t bank) {
	uint32_t proc = vbe_modeinfo.window_function;
	if (proc == 0) return;

	(void)bank;
	(void)proc;

#if TARGET_MSDOS == 32
	// TODO
#else
	__asm {
		mov	ax,0x4F05	; AH=4Fh AL=05h CPU memory window control
		mov	bx,0		; Set window, Window A
		mov	dx,bank		; window position in granularity units
		call	dword ptr [proc]
	}
#endif
}

///

static uint16_t current_bank = ~0u;

static void bnksw_int10(uint16_t bank) {
	vbe_bank_switch_int10(bank);
}

static void bnksw_rmwnfnc(uint16_t bank) {
	vbe_bank_switch_rmwnfunc(bank);
}

static void draw_scanline_bnksw(unsigned int y,unsigned char *src,unsigned int pixels) {
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
			if (cpy > pixels) cpy = pixels;
		}
		else {
			cpy = pixels;
		}

#if TARGET_MSDOS == 32
		memcpy(d,src,cpy);
#else
		_fmemcpy(d,src,cpy);
#endif
		pixels -= cpy;
		if (pixels != 0) {
			src += cpy;
			bank += window_bank_advance;

			if (current_bank != bank) {
				draw_scanline_bank_switch(bank);
				current_bank = bank;
			}

#if TARGET_MSDOS == 32
			d = (unsigned char*)0xA0000;
			memcpy(d,src,pixels);
#else
			d = MK_FP(0xA000,0);
			_fmemcpy(d,src,pixels);
#endif
		}
	}
}

static void help(void) {
	fprintf(stderr,"general <bmp file>\n");
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

int main(int argc,char **argv) {
	struct BMPFILEREAD *bfr;
	unsigned int dispw,i;
	uint16_t vbemode = 0;

	if (!parse_argv(argc,argv))
		return 1;

	if (!detect_vbe()) {
		fprintf(stderr,"VESA BIOS extensios not detected\n");
		return 1;
	}

	bfr = open_bmp(bmpfile);
	if (bfr == NULL) {
		fprintf(stderr,"Failed to open BMP, errno %s\n",strerror(errno));
		return 1;
	}
	if (bfr->bpp == 0 || bfr->bpp > 32 || bfr->colors == 0 || bfr->colors > 256 || bfr->palette == 0) {
		fprintf(stderr,"BMP wrong format\n");
		return 1;
	}

	/* TODO: This is where it might be handy to support the Windows 95 BI_BITFIELDS extension to distinguish 15/16bpp */
	fprintf(stderr,"BMP is %u x %u x %ubpp\n",bfr->width,bfr->height,bfr->bpp);
	if (bfr->colors != 0) fprintf(stderr,"With a %u color palette\n",bfr->colors);

	/* Now find a VESA BIOS mode that matches the bitmap */
	/* TODO: Including bitfields from BI_BITFIELDS */
	vbemode = find_vbe_mode(bfr->width,bfr->height,bfr->bpp);
	if (vbemode == 0) {
		fprintf(stderr,"No matching VESA BIOS mode\n");
		return 1;
	}

	img_width = vbe_modeinfo.x_resolution;
	img_height = vbe_modeinfo.y_resolution;
	img_stride = vbe_modeinfo.bytes_per_scan_line;

	fprintf(stderr,"Found matching VESA BIOS mode 0x%x\n",vbemode);

	if (window_size == 0 || window_mult == 0 || window_bank_advance == 0) {
		fprintf(stderr,"ERROR: Bad window values\n");
		return 1;
	}

	draw_scanline_bank_switch = bnksw_int10;
	if (vbe_modeinfo.window_function != 0) {
		fprintf(stderr,"Direct bank switching function available\n");
		draw_scanline_bank_switch = bnksw_rmwnfnc;
	}
	draw_scanline = draw_scanline_bnksw;

	if (!set_vbe_mode(vbemode)) {
		fprintf(stderr,"Unable to set mode\n");
		return 1;
	}

	if (vbe_info.capabilities & VBE_CAP_8BIT_DAC) {
		if (vbe_set_dac_width(8)) {
			img_dac_width = 8;
		}
	}

	/* set palette */
	if (vbe_modeinfo.bits_per_pixel == 8) {
		unsigned int shf = 2;

		if (img_dac_width == 8) shf = 0;

		outp(0x3C8,0);
		for (i=0;i < bfr->colors;i++) {
			outp(0x3C9,bfr->palette[i].rgbRed >> shf);
			outp(0x3C9,bfr->palette[i].rgbGreen >> shf);
			outp(0x3C9,bfr->palette[i].rgbBlue >> shf);
		}
	}

	/* load and render */
	dispw = bfr->width;
	if (dispw > img_width) dispw = img_width;
	while (read_bmp_line(bfr) == 0)
		draw_scanline((unsigned int)bfr->current_line,bfr->scanline,dispw);

	/* done reading */
	close_bmp(&bfr);

	/* wait for ENTER */
	while (getch() != 13);

	/* restore text */
	__asm {
		mov	ax,0x0003	; AH=0x00 AL=0x03
		int	0x10
	}
	return 0;
}

