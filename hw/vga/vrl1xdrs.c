
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/vga/vrl1xdrc.h>

void draw_vrl1_vgax_modexstretch(unsigned int x,unsigned int y,unsigned int xstep/*1/64 scale 10.6 fixed pt*/,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
#if TARGET_MSDOS == 32
	unsigned char *draw;
#else
	unsigned char far *draw;
#endif
	const unsigned int xmax = hdr->width << 6U;
	unsigned int vram_offset = (y * vga_state.vga_draw_stride) + (x >> 2),fx=0;
	unsigned char vga_plane = (x & 3);
	unsigned char *s;

	/* draw one by one */
	while (fx < xmax) {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		s = data + lineoffs[fx >> 6U];
		draw_vrl1_vgax_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		fx += xstep;
		if ((++vga_plane) == 4) {
			vram_offset++;
			vga_plane = 0;
		}
	}
}

