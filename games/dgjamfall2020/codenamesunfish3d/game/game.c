
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
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

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

    vrl_palrebase(vrl_header,vrl_lineoffs,buffer+sizeof(*vrl_header),0x20); /* adjust the image to start at palette 0x20 */

    return 0;
fail:
    if (buffer != NULL) free(buffer);
    if (fd >= 0) close(fd);
    return 1;
}

const char *image_file[] = {
    "sorcbwo1.vrl",
    "sorcbwo2.vrl",
    "sorcbwo3.vrl",
    "sorcbwo4.vrl",
    "sorcbwo5.vrl",
    "sorcbwo6.vrl",
    "sorcbwo7.vrl",
    "sorcbwo8.vrl",
    "sorcbwo9.vrl",
    "sorcuhhh.vrl",
    "sorcwoo1.vrl",
    "sorcwoo2.vrl",
    "sorcwoo3.vrl",
    "sorcwoo4.vrl",
    "sorcwoo5.vrl",
    "sorcwoo6.vrl",
    "sorcwoo7.vrl",
    "sorcwoo8.vrl",
    "sorcwoo9.vrl",
    NULL
};

void load_seq(void) {
    while (vrl_image_count < MAX_IMAGES && image_file[vrl_image_count] != NULL) {
        if (load_vrl(vrl_image_count,image_file[vrl_image_count])) break;
        vrl_image_count++;
    }
}

int main() {
    unsigned int cur_offset=0x4000,next_offset=0;
    VGA_RAM_PTR	orig_vga_graphics_ram;
    unsigned int dof;
    int redraw = 1;
    int c;

    probe_dos();
	cpu_probe();
    if (cpu_basic_level < 3) {
        printf("This game requires a 386 or higher\n");
        return 1;
    }

	if (!probe_8254()) {
		printf("No 8254 detected.\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("No 8259 detected.\n");
		return 1;
	}
    if (!probe_vga()) {
        printf("VGA probe failed.\n");
        return 1;
    }

#if TARGET_MSDOS == 16
    probe_emm();            // expanded memory support
    probe_himem_sys();      // extended memory support

    if (emm_present) {
        printf("Expanded memory present.\n");
    }
    if (himem_sys_present) {
        printf("Extended memory present.\n");
    }
#endif

    load_seq();
    if (vrl_image_count == 0)
        return 1;

    int10_setmode(19);
    update_state_from_vga();
    vga_enable_256color_modex(); // VGA mode X
    orig_vga_graphics_ram = vga_state.vga_graphics_ram;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + next_offset;
    vga_set_start_location(cur_offset);

    {
        int fd;

        /* load color palette */
        fd = open("sorcwoo.pal",O_RDONLY|O_BINARY);
        if (fd >= 0) {
            unsigned int i;

            read(fd,palette,768);
            close(fd);

            vga_palette_lseek(0+0x20);
            for (i=0;i < 256;i++) vga_palette_write(palette[(i*3)+0]>>2,palette[(i*3)+1]>>2,palette[(i*3)+2]>>2);
        }
    }

    do {
        if (redraw) {
            redraw = 0;

            vga_wait_for_vsync();
            vga_wait_for_vsync_end();
            vga_wait_for_hsync_end();

            vga_write_sequencer(0x02/*map mask*/,0xF);
            vga_state.vga_graphics_ram = orig_vga_graphics_ram + next_offset;
            for (dof=0;dof < 0x4000;dof++) vga_state.vga_graphics_ram[dof] = 0;

            draw_vrl1_vgax_modex(0,0,
                vrl_image[vrl_image_select].vrl_header,
                vrl_image[vrl_image_select].vrl_lineoffs,
                vrl_image[vrl_image_select].buffer+sizeof(*vrl_image[vrl_image_select].vrl_header),
                vrl_image[vrl_image_select].bufsz-sizeof(*vrl_image[vrl_image_select].vrl_header));

            cur_offset = next_offset;
            next_offset = (next_offset ^ 0x4000) & 0x7FFF;
            vga_set_start_location(cur_offset);
            vga_state.vga_graphics_ram = orig_vga_graphics_ram + next_offset;
        }

        if (kbhit()) {
            c = getch();
            if (c == 27) break;
        }

        redraw = 1;
        if ((++vrl_image_select) >= vrl_image_count)
            vrl_image_select = 0;
    } while (1);

    int10_setmode(3);
    return 0;
}

