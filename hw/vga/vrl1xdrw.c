
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

void draw_vrl1_vgax_modex(unsigned int x,unsigned int y,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
#if TARGET_MSDOS == 32
	unsigned char *draw;
#else
	unsigned char far *draw;
#endif
	unsigned int vram_offset = (y * vga_stride) + (x >> 2),sx;
	unsigned char vga_plane = (x & 3);
	unsigned char *s;

	/* draw one by one */
	for (sx=0;sx < hdr->width;sx++) {
		draw = vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		s = data + lineoffs[sx];
		draw_vrl1_vgax_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		if ((++vga_plane) == 4) {
			vram_offset++;
			vga_plane = 0;
		}
	}
}

