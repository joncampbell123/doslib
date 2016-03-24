
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

#pragma pack(push,1)
struct vrl_header {
	uint8_t			vrl_sig[4];		// +0x00  "VRL1"
	uint8_t			fmt_sig[4];		// +0x04  "VGAX"
	uint16_t		height;			// +0x08  Sprite height
	uint16_t		width;			// +0x0A  Sprite width
	int16_t			hotspot_x;		// +0x0C  Hotspot offset (X) for programmer's reference
	int16_t			hotspot_y;		// +0x0E  Hotspot offset (Y) for programmer's reference
};							// =0x10
#pragma pack(pop)

static unsigned char palette[768];

unsigned int *vrl_genlineoffsets(struct vrl_header *hdr,unsigned char *data,int datasz) {
	unsigned int x = 0;
	unsigned char run,skip;
	unsigned char *fence = data + datasz,*s;
	unsigned int *list = malloc(hdr->width * sizeof(unsigned int));
	if (list == NULL) return NULL;

	s = data;
	while (s < fence) {
		list[x++] = (unsigned int)(s - data);
		while (s < fence) {
			run = *s++;
			if (run == 0xFF) break;
			skip = *s++;

			if (run&0x80)
				s++;
			else
				s += run;
		}

		if (x >= hdr->width) break;
	}

	return list;
}

static inline void draw_vrl_modex_strip(unsigned char far *draw,unsigned char *s) {
	unsigned char run,skip,b;

	do {
		run = *s++;
		if (run == 0xFF) break;
		skip = *s++;
		draw += skip * vga_state.vga_stride;

		if (run & 0x80) {
			b = *s++;
			while (run > 0x80) {
				*draw = b;
				draw += vga_state.vga_stride;
				run--;
			}
		}
		else {
			while (run > 0) {
				*draw = *s++;
				draw += vga_state.vga_stride;
				run--;
			}
		}
	} while (1);
}

static inline void draw_vrl_modex_stripystretch(unsigned char far *draw,unsigned char *s,unsigned int ystretch/*10.6 fixed pt*/) {
	unsigned char run,skip,b,fy=0;

	do {
		run = *s++;
		if (run == 0xFF) break;
		skip = *s++;
		while (skip > 0) {
			while (fy < (1 << 6)) {
				draw += vga_state.vga_stride;
				fy += ystretch;
			}
			fy -= 1 << 6;
			skip--;
		}

		if (run & 0x80) {
			b = *s++;
			while (run > 0x80) {
				while (fy < (1 << 6)) {
					*draw = b;
					draw += vga_state.vga_stride;
					fy += ystretch;
				}
				fy -= 1 << 6;
				run--;
			}
		}
		else {
			while (run > 0) {
				while (fy < (1 << 6)) {
					*draw = *s;
					draw += vga_state.vga_stride;
					fy += ystretch;
				}
				fy -= 1 << 6;
				run--;
				s++;
			}
		}
	} while (1);
}

void draw_vrl_modex(unsigned int x,unsigned int y,struct vrl_header *hdr,unsigned int *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
	unsigned int vram_offset = (y * vga_state.vga_stride) + (x >> 2),sx;
	unsigned char vga_plane = (x & 3);
	unsigned char far *draw;
	unsigned char *s;

	/* draw one by one */
	for (sx=0;sx < hdr->width;sx++) {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		s = data + lineoffs[sx];
		draw_vrl_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		if ((++vga_plane) == 4) {
			vram_offset++;
			vga_plane = 0;
		}
	}

	vga_write_sequencer(0x02/*map mask*/,0xF);
}

void draw_vrl_modexstretch(unsigned int x,unsigned int y,unsigned int xstretch/*1/64 scale 10.6 fixed pt*/,struct vrl_header *hdr,unsigned int *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
	unsigned int vram_offset = (y * vga_state.vga_stride) + (x >> 2),fx=0;
	unsigned char vga_plane = (x & 3);
	unsigned int limit = vga_state.vga_stride;
	unsigned char far *draw;
	unsigned char *s;

	/* draw one by one */
	do {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		{
			unsigned int x = fx >> 6;
			if (x >= hdr->width) break;
			s = data + lineoffs[x];
		}
		draw_vrl_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		fx += xstretch;
		if ((++vga_plane) == 4) {
			if (--limit == 0) break;
			vram_offset++;
			vga_plane = 0;
		}
	} while(1);

	vga_write_sequencer(0x02/*map mask*/,0xF);
}

void draw_vrl_modexystretch(unsigned int x,unsigned int y,unsigned int xstretch/*1/64 scale 10.6 fixed pt*/,unsigned int ystretch/*1/6 scale 10.6*/,struct vrl_header *hdr,unsigned int *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
	unsigned int vram_offset = (y * vga_state.vga_stride) + (x >> 2),fx=0;
	unsigned char vga_plane = (x & 3);
	unsigned int limit = vga_state.vga_stride;
	unsigned char far *draw;
	unsigned char *s;

	/* draw one by one */
	do {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		{
			unsigned int x = fx >> 6;
			if (x >= hdr->width) break;
			s = data + lineoffs[x];
		}
		draw_vrl_modex_stripystretch(draw,s,ystretch);

		/* end of a vertical strip. next line? */
		fx += xstretch;
		if ((++vga_plane) == 4) {
			if (--limit == 0) break;
			vram_offset++;
			vga_plane = 0;
		}
	} while(1);

	vga_write_sequencer(0x02/*map mask*/,0xF);
}

int main(int argc,char **argv) {
	struct vrl_header *vrl_header;
	unsigned int *vrl_lineoffs;
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
		if (sz < sizeof(vrl_header)) return 1;
		if (sz >= 65535UL) return 1;

		bufsz = (unsigned int)sz;
		buffer = malloc(bufsz);
		if (buffer == NULL) return 1;

		lseek(fd,0,SEEK_SET);
		if ((unsigned int)read(fd,buffer,bufsz) < bufsz) return 1;

		vrl_header = (struct vrl_header*)buffer;
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
	vrl_lineoffs = vrl_genlineoffsets(vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	if (vrl_lineoffs == NULL) return 1;

	draw_vrl_modex(0,0,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	while (getch() != 13);

	{
		unsigned int i;

		for (i=1;i < 320;i++)
			draw_vrl_modex(i,0,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	{
		unsigned int i;

		for (i=1;i < 200;i++)
			draw_vrl_modex(i,i,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	{
		unsigned int i;

		for (i=(2 << 6)/*200%*/;i >= (1 << 4)/*25%*/;i--)
			draw_vrl_modexstretch(0,0,i,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);
	{
		unsigned int i;

		for (i=(2 << 6)/*200%*/;i >= (1 << 4)/*25%*/;i--)
			draw_vrl_modexystretch(0,0,i,i,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	int10_setmode(3);
	free(vrl_lineoffs);
	buffer = NULL;
	free(buffer);
	bufsz = 0;
	return 0;
}

