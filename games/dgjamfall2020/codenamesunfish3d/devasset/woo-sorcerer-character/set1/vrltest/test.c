
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

	draw_vrl1_vgax_modex(0,0,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	while (getch() != 13);

	{
		unsigned int i;

		for (i=1;i < 320;i++)
			draw_vrl1_vgax_modex(i,0,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	{
		unsigned int i;

		for (i=1;i < 200;i++)
			draw_vrl1_vgax_modex(i,i,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	{
		unsigned int i;

		for (i=(2 << 6)/*200%*/;i >= (1 << 4)/*25%*/;i--)
			draw_vrl1_vgax_modexstretch(0,0,i,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);
	{
		unsigned int i;

		for (i=(2 << 6)/*200%*/;i >= (1 << 4)/*25%*/;i--)
			draw_vrl1_vgax_modexystretch(0,0,i,i,vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	}
	while (getch() != 13);

	int10_setmode(3);
	free(vrl_lineoffs);
	buffer = NULL;
	free(buffer);
	bufsz = 0;
	return 0;
}

