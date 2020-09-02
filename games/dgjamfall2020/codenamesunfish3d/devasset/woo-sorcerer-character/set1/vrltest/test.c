
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

#define MAX_IMAGES          64

struct vrl_image {
	struct vrl1_vgax_header*        vrl_header;
	vrl1_vgax_offset_t*             vrl_lineoffs;
	unsigned char*                  buffer;
	unsigned int                    bufsz;
};

int                     vrl_image_count = 0;
int                     vrl_image_select = 0;
struct vrl_image        vrl_image[MAX_IMAGES];

int load_vrl(int slot,const char *path) {
	struct vrl1_vgax_header *vrl_header;
	vrl1_vgax_offset_t *vrl_lineoffs;
	unsigned char *buffer = NULL;
	unsigned int bufsz = 0;
	int fd = -1;

	fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Unable to open '%s'\n",path);
		goto fail;
	}
	{
		unsigned long sz = lseek(fd,0,SEEK_END);
		if (sz < sizeof(*vrl_header)) goto fail;
		if (sz >= 65535UL) goto fail;

		bufsz = (unsigned int)sz;
		buffer = malloc(bufsz);
		if (buffer == NULL) goto fail;

		lseek(fd,0,SEEK_SET);
		if ((unsigned int)read(fd,buffer,bufsz) < bufsz) goto fail;

		vrl_header = (struct vrl1_vgax_header*)buffer;
		if (memcmp(vrl_header->vrl_sig,"VRL1",4) || memcmp(vrl_header->fmt_sig,"VGAX",4)) goto fail;
		if (vrl_header->width == 0 || vrl_header->height == 0) goto fail;
	}
	close(fd);

	/* preprocess the sprite to generate line offsets */
	vrl_lineoffs = vrl1_vgax_genlineoffsets(vrl_header,buffer+sizeof(*vrl_header),bufsz-sizeof(*vrl_header));
	if (vrl_lineoffs == NULL) goto fail;

    vrl_image[slot].vrl_header = vrl_header;
    vrl_image[slot].vrl_lineoffs = vrl_lineoffs;
    vrl_image[slot].buffer = buffer;
    vrl_image[slot].bufsz = bufsz;

    return 0;
fail:
    if (buffer != NULL) free(buffer);
    if (fd >= 0) close(fd);
    return 1;
}

void load_seq(void) {
    char tmp[20];

    while (vrl_image_count < MAX_IMAGES) {
        sprintf(tmp,"%04d.vrl",vrl_image_count);
        if (load_vrl(vrl_image_count,tmp)) break;
        vrl_image_count++;
    }
}

int main() {
    int c;

    load_seq();
    if (vrl_image_count == 0)
        return 1;

	probe_dos();
	if (!probe_vga()) {
		printf("VGA probe failed\n");
		return 1;
	}
	int10_setmode(19);
	update_state_from_vga();
	vga_enable_256color_modex(); // VGA mode X

    {
        int fd;

        /* load color palette */
        fd = open("palette.pal",O_RDONLY|O_BINARY);
        if (fd >= 0) {
            unsigned int i;

            read(fd,palette,768);
            close(fd);

            vga_palette_lseek(0);
            for (i=0;i < 256;i++) vga_palette_write(palette[(i*3)+0]>>2,palette[(i*3)+1]>>2,palette[(i*3)+2]>>2);
        }
    }

    do {
    	draw_vrl1_vgax_modex(0,0,
            vrl_image[vrl_image_select].vrl_header,
            vrl_image[vrl_image_select].vrl_lineoffs,
            vrl_image[vrl_image_select].buffer+sizeof(*vrl_image[vrl_image_select].vrl_header),
            vrl_image[vrl_image_select].bufsz-sizeof(*vrl_image[vrl_image_select].vrl_header));

        c = getch();
        if (c == 27) break;

        if (c == ',' || c == '<') {
            if (--vrl_image_select < 0)
                vrl_image_select = vrl_image_count - 1;
        }
        else if (c == '.' || c == '>') {
            if (++vrl_image_select >= vrl_image_count)
                vrl_image_select = 0;
        }
    } while (1);

	int10_setmode(3);
	return 0;
}

