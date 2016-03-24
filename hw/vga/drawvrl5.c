
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

static unsigned char palette[768];

int main(int argc,char **argv) {
	struct vrl1_vgax_header *vrl_header;
	vrl1_vgax_offset_t *vrl_lineoffs;
	unsigned char *buffer;
	unsigned int bufsz;
	int fd;

	if (argc < 3) {
		fprintf(stderr,"drawvrl <VRL file> <palette file>\n");
		return 1;
	}

	fd = open(argv[1],O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Unable to open '%s'\n",argv[1]);
		return 1;
	}
	{
		unsigned long sz = lseek(fd,0,SEEK_END);
		if (sz < sizeof(*vrl_header)) return 1;
		if (sz >= 65535UL) return 1;

		bufsz = (unsigned int)sz;
		buffer = malloc(bufsz);
		if (buffer == NULL) return 1;

		lseek(fd,0,SEEK_SET);
		if ((unsigned int)read(fd,buffer,bufsz) < bufsz) return 1;

		vrl_header = (struct vrl1_vgax_header*)buffer;
		if (memcmp(vrl_header->vrl_sig,"VRL1",4) || memcmp(vrl_header->fmt_sig,"VGAX",4)) return 1;
		if (vrl_header->width == 0 || vrl_header->height == 0) return 1;
	}
	close(fd);

	probe_dos();
	if (!probe_vga()) {
		printf("VGA probe failed\n");
		return 1;
	}
	int10_setmode(19);
	update_state_from_vga();
	vga_enable_256color_modex(); // VGA mode X
	vga_state.vga_width = 320; // VGA lib currently does not update this
	vga_state.vga_height = 200; // VGA lib currently does not update this

#if 1 // 320x240 test mode: this is how Project 16 is using our code, enable for test case
	{
		struct vga_mode_params cm;

		vga_read_crtc_mode(&cm);

		// 320x240 mode 60Hz
		cm.vertical_total = 525;
		cm.vertical_start_retrace = 0x1EA;
		cm.vertical_end_retrace = 0x1EC;
		cm.vertical_display_end = 480;
		cm.vertical_blank_start = 489;
		cm.vertical_blank_end = 517;

		vga_write_crtc_mode(&cm,0);
	}
	vga_state.vga_height = 240; // VGA lib currently does not update this
#endif

	/* load color palette */
	fd = open(argv[2],O_RDONLY|O_BINARY);
	if (fd >= 0) {
		unsigned int i;

		read(fd,palette,768);
		close(fd);

		vga_palette_lseek(0);
		for (i=0;i < 256;i++) vga_palette_write(palette[(i*3)+0]>>2,palette[(i*3)+1]>>2,palette[(i*3)+2]>>2);
	}

	/* preprocess the sprite to generate line offsets */
	vrl_lineoffs = vrl1_vgax_genlineoffsets(vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	if (vrl_lineoffs == NULL) return 1;

	{
		unsigned int i,j,o;

		/* fill screen with a distinctive pattern */
		for (i=0;i < vga_state.vga_width;i++) {
			o = i >> 2;
			vga_write_sequencer(0x02/*map mask*/,1 << (i&3));
			for (j=0;j < vga_state.vga_height;j++,o += vga_state.vga_stride)
				vga_state.vga_graphics_ram[o] = (i^j)&15; // VRL samples put all colors in first 15!
		}
	}
	while (getch() != 13);

	/* make distinctive pattern offscreen, render sprite, copy onscreen */
	{
		const unsigned int offscreen_ofs = (vga_state.vga_stride * vga_state.vga_height);
		unsigned int i,j,o,o2,x,y,rx,ry,w,h;
		unsigned int overdraw = 1;	// how many pixels to "overdraw" so that moving sprites with edge pixels don't leave streaks.
						// if the sprite's edge pixels are clear anyway, you can set this to 0.
		VGA_RAM_PTR omemptr;
		int xdir=1,ydir=1;

		/* starting coords. note: this technique is limited to x coordinates of multiple of 4 */
		x = 0;
		y = 0;

		/* do it */
		omemptr = vga_state.vga_graphics_ram; // save original mem ptr
		while (1) {
			/* stop animating if the user hits ENTER */
			if (kbhit()) {
				if (getch() == 13) break;
			}

			/* render box bounds. y does not need modification, but x and width must be multiple of 4 */
			if (x >= overdraw) rx = (x - overdraw) & (~3);
			else rx = 0;
			if (y >= overdraw) ry = (y - overdraw);
			else ry = 0;
			h = vrl_header->height + overdraw + y - ry;
			w = (x + vrl_header->width + (overdraw*2) + 3/*round up*/ - rx) & (~3);
			if ((rx+w) > vga_state.vga_width) w = vga_state.vga_width-rx;
			if ((ry+h) > vga_state.vga_height) h = vga_state.vga_height-ry;

			/* replace VGA stride with our own and mem ptr. then sprite rendering at this stage is just (0,0) */
			vga_state.vga_draw_stride_limit = (vga_state.vga_width + 3/*round up*/ - x) >> 2;
			vga_state.vga_draw_stride = w >> 2;
			vga_state.vga_graphics_ram = omemptr + offscreen_ofs;

			/* first draw pattern corresponding to that part of the screen. this COULD be optimized, obviously, but it's designed for study.
			 * also note we don't have to use the same stride as the display! */
			for (i=rx;i < (rx+w);i++) {
				o = (i-rx) >> 2;
				vga_write_sequencer(0x02/*map mask*/,1 << (i&3));
				for (j=ry;j < (ry+h);j++,o += vga_state.vga_draw_stride)
					vga_state.vga_graphics_ram[o] = (i^j)&15; // VRL samples put all colors in first 15!
			}

			/* then the sprite. note modding ram ptr means we just draw to (x&3,0) */
			draw_vrl1_vgax_modex(x-rx,y-ry,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));

			/* restore ptr */
			vga_state.vga_graphics_ram = omemptr;

			/* block copy to visible RAM from offscreen */
			vga_setup_wm1_block_copy();
			o = offscreen_ofs; // source offscreen
			o2 = (ry * vga_state.vga_stride) + (rx >> 2); // dest visible (original stride)
			for (i=0;i < h;i++,o += vga_state.vga_draw_stride,o2 += vga_state.vga_stride) vga_wm1_mem_block_copy(o2,o,w >> 2);
			/* must restore Write Mode 0/Read Mode 0 for this code to continue drawing normally */
			vga_restore_rm0wm0();

			/* restore stride */
			vga_state.vga_draw_stride_limit = vga_state.vga_draw_stride = vga_state.vga_stride;

			/* step */
			x += xdir;
			y += ydir;
			if (x >= (vga_state.vga_width - 1) || x == 0)
				xdir = -xdir;
			if (y >= (vga_state.vga_height - 1) || y == 0)
				ydir = -ydir;
		}
	}

	/* make distinctive pattern offscreen, render sprite, copy onscreen.
	 * this time, we render the distinctive pattern to another offscreen location and just copy.
	 * note this version is much faster too! */
	{
		const unsigned int offscreen_ofs = (vga_state.vga_stride * vga_state.vga_height);
		const unsigned int pattern_ofs = 0x10000UL - (vga_state.vga_stride * vga_state.vga_height);
		unsigned int i,j,o,o2,x,y,rx,ry,w,h;
		unsigned int overdraw = 1;	// how many pixels to "overdraw" so that moving sprites with edge pixels don't leave streaks.
						// if the sprite's edge pixels are clear anyway, you can set this to 0.
		VGA_RAM_PTR omemptr;
		int xdir=1,ydir=1;

		/* fill pattern offset with a distinctive pattern */
		for (i=0;i < vga_state.vga_width;i++) {
			o = (i >> 2) + pattern_ofs;
			vga_write_sequencer(0x02/*map mask*/,1 << (i&3));
			for (j=0;j < vga_state.vga_height;j++,o += vga_state.vga_stride)
				vga_state.vga_graphics_ram[o] = (i^j)&15; // VRL samples put all colors in first 15!
		}

		/* starting coords. note: this technique is limited to x coordinates of multiple of 4 */
		x = 0;
		y = 0;

		/* do it */
		omemptr = vga_state.vga_graphics_ram; // save original mem ptr
		while (1) {
			/* stop animating if the user hits ENTER */
			if (kbhit()) {
				if (getch() == 13) break;
			}

			/* render box bounds. y does not need modification, but x and width must be multiple of 4 */
			if (x >= overdraw) rx = (x - overdraw) & (~3);
			else rx = 0;
			if (y >= overdraw) ry = (y - overdraw);
			else ry = 0;
			h = vrl_header->height + overdraw + y - ry;
			w = (x + vrl_header->width + (overdraw*2) + 3/*round up*/ - rx) & (~3);
			if ((rx+w) > vga_state.vga_width) w = vga_state.vga_width-rx;
			if ((ry+h) > vga_state.vga_height) h = vga_state.vga_height-ry;

			/* block copy pattern to where we will draw the sprite */
			vga_setup_wm1_block_copy();
			o2 = offscreen_ofs;
			o = pattern_ofs + (ry * vga_state.vga_stride) + (rx >> 2); // source offscreen
			for (i=0;i < h;i++,o += vga_state.vga_stride,o2 += (w >> 2)) vga_wm1_mem_block_copy(o2,o,w >> 2);
			/* must restore Write Mode 0/Read Mode 0 for this code to continue drawing normally */
			vga_restore_rm0wm0();

			/* replace VGA stride with our own and mem ptr. then sprite rendering at this stage is just (0,0) */
			vga_state.vga_draw_stride_limit = (vga_state.vga_width + 3/*round up*/ - x) >> 2;
			vga_state.vga_draw_stride = w >> 2;
			vga_state.vga_graphics_ram = omemptr + offscreen_ofs;

			/* then the sprite. note modding ram ptr means we just draw to (x&3,0) */
			draw_vrl1_vgax_modex(x-rx,y-ry,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));

			/* restore ptr */
			vga_state.vga_graphics_ram = omemptr;

			/* block copy to visible RAM from offscreen */
			vga_setup_wm1_block_copy();
			o = offscreen_ofs; // source offscreen
			o2 = (ry * vga_state.vga_stride) + (rx >> 2); // dest visible (original stride)
			for (i=0;i < h;i++,o += vga_state.vga_draw_stride,o2 += vga_state.vga_stride) vga_wm1_mem_block_copy(o2,o,w >> 2);
			/* must restore Write Mode 0/Read Mode 0 for this code to continue drawing normally */
			vga_restore_rm0wm0();

			/* restore stride */
			vga_state.vga_draw_stride_limit = vga_state.vga_draw_stride = vga_state.vga_stride;

			/* step */
			x += xdir;
			y += ydir;
			if (x >= (vga_state.vga_width - 1) || x == 0)
				xdir = -xdir;
			if (y >= (vga_state.vga_height - 1) || y == 0)
				ydir = -ydir;
		}
	}

	/* another handy "demo" effect using VGA write mode 1.
	 * we can take what's on screen and vertically squash it like an old analog TV set turning off. */
	{
		unsigned int blank_line_ofs = (vga_state.vga_stride * vga_state.vga_height * 2);
		unsigned int copy_ofs = (vga_state.vga_stride * vga_state.vga_height);
		unsigned int display_ofs = 0x0000;
		unsigned int i,y,soh,doh,dstart;
		unsigned int dh_blankfill = 8;
		unsigned int dh_step = 8;
		uint32_t sh,dh,yf,ystep;

		/* copy active display (0) to offscreen buffer (0x4000) */
		vga_state.vga_draw_stride_limit = vga_state.vga_draw_stride = vga_state.vga_stride;
		vga_setup_wm1_block_copy();
		vga_wm1_mem_block_copy(copy_ofs,display_ofs,vga_state.vga_stride * vga_state.vga_height);
		vga_restore_rm0wm0();

		/* need a blank line as well */
		for (i=0;i < vga_state.vga_stride;i++) vga_state.vga_graphics_ram[i+blank_line_ofs] = 0;

		sh = dh = vga_state.vga_height;
		while (dh >= dh_step) {
			/* stop animating if the user hits ENTER */
			if (kbhit()) {
				if (getch() == 13) break;
			}

			/* wait for vsync end */
			vga_wait_for_vsync_end();

			/* what scalefactor to use for stretching? */
			ystep = (0x10000UL * sh) / dh;
			dstart = (vga_state.vga_height - dh) / 2; // center the squash effect on screen, otherwise it would squash to top of screen
			doh = display_ofs;
			soh = copy_ofs;
			yf = 0;
			y = 0;

			/* for performance, keep VGA in write mode 1 the entire render */
			vga_setup_wm1_block_copy();

			/* blank lines */
			if (dstart >= dh_blankfill) y = dstart - dh_blankfill;
			else y = 0;
			doh = vga_state.vga_stride * y;

			while (y < dstart) {
				vga_wm1_mem_block_copy(doh,blank_line_ofs,vga_state.vga_stride);
				doh += vga_state.vga_stride;
				y++;
			}

			/* draw */
			while (y < (dh+dstart)) {
				soh = copy_ofs + ((yf >> 16UL) * vga_state.vga_stride);
				vga_wm1_mem_block_copy(doh,soh,vga_state.vga_stride);
				doh += vga_state.vga_stride;
				yf += ystep;
				y++;
			}

			/* blank lines */
			while (y < vga_state.vga_height && y < (dh+dstart+dh_blankfill)) {
				vga_wm1_mem_block_copy(doh,blank_line_ofs,vga_state.vga_stride);
				doh += vga_state.vga_stride;
				y++;
			}

			/* done */
			vga_restore_rm0wm0();

			/* wait for vsync */
			vga_wait_for_vsync();

			/* make it shrink */
			dh -= dh_step;
			if (dh < 40) dh_step = 1;
		}
	}

	int10_setmode(3);
	free(vrl_lineoffs);
	buffer = NULL;
	free(buffer);
	bufsz = 0;
	return 0;
}

