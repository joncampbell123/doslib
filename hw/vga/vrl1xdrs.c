
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

void draw_vrl1_vgax_modexstretch(unsigned int x,unsigned int y,unsigned int xstretch/*1/64 scale 10.6 fixed pt*/,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
	unsigned int vram_offset = (y * vga_stride) + (x >> 2),fx=0;
	unsigned char vga_plane = (x & 3);
	unsigned int limit = vga_stride;
	unsigned char far *draw;
	unsigned char *s;

	if (limit > hdr->width) limit = hdr->width;

	/* draw one by one */
	do {
		draw = vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		{
			const unsigned int x = fx >> 6;
			if (x >= hdr->width) break;
			s = data + lineoffs[x];
		}
		draw_vrl1_vgax_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		fx += xstretch;
		if ((++vga_plane) == 4) {
			if (--limit == 0) break;
			vram_offset++;
			vga_plane = 0;
		}
	} while(1);
}

