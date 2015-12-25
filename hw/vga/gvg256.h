
#include <stdint.h>

#include <hw/vga/vga.h>

struct v320x200x256_VGA_state {
	uint16_t		stride;			// bytes per scanline
	uint16_t		width,virt_width;	// video width vs virtual width
	uint16_t		height,virt_height;	// video height vs virtual height
	uint16_t		draw_offset;		// draw offset
	uint16_t		vis_offset;		// visual offset
	uint32_t		vram_size;		// size of VRAM
	VGA_RAM_PTR		draw_ptr;
};

extern struct v320x200x256_VGA_state v320x200x256_VGA_state;

