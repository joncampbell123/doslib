
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

void draw_vrl_modex(unsigned int x,unsigned int y,struct vrl_header *hdr,unsigned char *data,unsigned int datasz) {
	unsigned int vram_offset = (y * vga_stride) + (x >> 2);
	unsigned char *fence = data + datasz;
	unsigned char vga_plane = (x & 3);
	unsigned char run,skip,b;
	unsigned char far *draw;

	while (data < fence) {
		/* start of another vertical strip */
		draw = vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);

		while (data < fence) {
			run = *data++;
			if (run == 0xFF) break;
			skip = *data++;
			draw += skip * vga_stride;
			if (run & 0x80) {
				b = *data++;
				while (run > 0x80) {
					*draw = b;
					draw += vga_stride;
					run--;
				}
			}
			else {
				while (run > 0) {
					*draw = *data++;
					draw += vga_stride;
					run--;
				}
			}
		}

		/* end of a vertical strip. next line? */
		if ((++vga_plane) == 4) {
			vram_offset++;
			vga_plane = 0;
		}
	}

	vga_write_sequencer(0x02/*map mask*/,0xF);
}

int main(int argc,char **argv) {
	struct vrl_header *vrl_header;
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

	draw_vrl_modex(0,0,vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	while (getch() != 13);

	{
		unsigned int i;

		for (i=1;i < 320;i++)
			draw_vrl_modex(i,0,vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	{
		unsigned int i;

		for (i=1;i < 200;i++)
			draw_vrl_modex(i,i,vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	int10_setmode(3);
	buffer = NULL;
	free(buffer);
	bufsz = 0;
	return 0;
}

