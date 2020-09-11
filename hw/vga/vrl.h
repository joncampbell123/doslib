
#ifndef __DOSLIB_HW_VGA_VRL_H
#define __DOSLIB_HW_VGA_VRL_H

#include <stdint.h>

#pragma pack(push,1)
struct vrl1_vgax_header {
	uint8_t			vrl_sig[4];		// +0x00  "VRL1"
	uint8_t			fmt_sig[4];		// +0x04  "VGAX"
	uint16_t		height;			// +0x08  Sprite height
	uint16_t		width;			// +0x0A  Sprite width
	int16_t			hotspot_x;		// +0x0C  Hotspot offset (X) for programmer's reference
	int16_t			hotspot_y;		// +0x0E  Hotspot offset (Y) for programmer's reference
};							// =0x10
#pragma pack(pop)

#if TARGET_MSDOS == 32
# define VRL_MAX_SIZE		(0x40000UL)		// 256KB
typedef uint32_t		vrl1_vgax_offset_t;
#else
# define VRL_MAX_SIZE		(0xFFF0UL)		// 64KB - 16
typedef uint16_t		vrl1_vgax_offset_t;
#endif

vrl1_vgax_offset_t *vrl1_vgax_genlineoffsets(struct vrl1_vgax_header *hdr,unsigned char *data,unsigned int datasz);
void draw_vrl1_vgax_modex(unsigned int x,unsigned int y,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz);
void draw_vrl1_vgax_modexstretch(unsigned int x,unsigned int y,unsigned int xstep/*1/64 scale 10.6 fixed pt*/,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz);
void draw_vrl1_vgax_modexystretch(unsigned int x,unsigned int y,unsigned int xstep/*1/64 scale 10.6 fixed pt*/,unsigned int ystep/*1/6 scale 10.6*/,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz);

/* add a constant value to all pixels in the VRL for palette management purposes */
#if TARGET_MSDOS == 32 || defined(__GNUC__)
void vrl_palrebase(struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs,unsigned char *buffer,const unsigned char adj);
#else
void vrl_palrebase(struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs,unsigned char far *buffer,const unsigned char adj);
#endif

#endif //__DOSLIB_HW_VGA_VRL_H

