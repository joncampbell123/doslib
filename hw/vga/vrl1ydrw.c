
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

#if TARGET_MSDOS == 32
static inline void draw_vrl1_vgax_modex_stripystretch(unsigned char *draw,unsigned char *s,unsigned int ystep/*10.6 fixed pt*/) {
#else
static inline void draw_vrl1_vgax_modex_stripystretch(unsigned char far *draw,unsigned char *s,unsigned int ystep/*10.6 fixed pt*/) {
#endif
	unsigned char run,skip,b;
	unsigned int fy=0,ym;

	do {
		run = *s++;
		if (run == 0xFF) break;
		skip = *s++;
		if (skip > 0) {
			ym = (unsigned int)skip << 6U;
			while (fy < ym) {
				draw += vga_state.vga_draw_stride;
				fy += ystep;
			}
			fy -= ym;
		}

		if (run & 0x80) {
			b = *s++;
			ym = ((unsigned int)(run - 0x80)) << 6U;
			while (fy < ym) {
				*draw = b;
				draw += vga_state.vga_draw_stride;
				fy += ystep;
			}
			fy -= ym;
		}
		else {
			while (run > 0) {
				while (fy < (1 << 6)) {
					*draw = *s;
					draw += vga_state.vga_draw_stride;
					fy += ystep;
				}
				fy -= 1 << 6;
				run--;
				s++;
			}
		}
	} while (1);
}

void draw_vrl1_vgax_modexystretch(unsigned int x,unsigned int y,unsigned int xstretch/*1/64 scale 10.6 fixed pt*/,unsigned int ystep/*1/6 scale 10.6*/,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
#if TARGET_MSDOS == 32
	unsigned char *draw;
#else
	unsigned char far *draw;
#endif
	const unsigned int xmax = hdr->width << 6U;
	unsigned int vram_offset = (y * vga_state.vga_draw_stride) + (x >> 2),fx=0;
	unsigned int vramlimit = vga_state.vga_draw_stride_limit;
	unsigned char vga_plane = (x & 3);
	unsigned char *s;

	/* draw one by one */
	while (fx < xmax) {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		s = data + lineoffs[fx >> 6U];
		draw_vrl1_vgax_modex_stripystretch(draw,s,ystep);

		/* end of a vertical strip. next line? */
		fx += xstretch;
		if ((++vga_plane) == 4) {
			if (--vramlimit == 0) break;
			vram_offset++;
			vga_plane = 0;
		}
	}
}

