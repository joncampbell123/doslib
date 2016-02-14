
#include <stdint.h>

#include <hw/vga/vga.h>

struct v320x200x256_VGA_state {
	uint16_t		stride;			// bytes per scanline
	uint16_t		width,virt_width;	// video width vs virtual width
	uint16_t		height,virt_height;	// video height vs virtual height
	uint32_t		draw_offset;		// draw offset
	uint32_t		vis_offset;		// visual offset
	uint32_t		vram_size;		// size of VRAM
	uint8_t			hpel;			// horizontal pel
	uint8_t			stride_shift;		// reflects VGA CRTC byte/word/dword mode
	VGA_RAM_PTR		draw_ptr;
	VGA_RAM_PTR		vis_ptr;
};

extern struct v320x200x256_VGA_state v320x200x256_VGA_state;

