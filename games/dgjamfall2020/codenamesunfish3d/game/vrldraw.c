
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
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

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"

#include <hw/vga/vrl1xdrc.h>

void draw_vrl1_vgax_modexclip(unsigned int x,unsigned int y,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
#if TARGET_MSDOS == 32
	unsigned char *draw;
#else
	unsigned char far *draw;
#endif
	unsigned int vram_offset = (y * vga_state.vga_draw_stride) + (x >> 2),sx;
	unsigned int vramlimit = vga_state.vga_draw_stride_limit;
	unsigned char vga_plane = (x & 3);
	unsigned char *s;

    (void)datasz;

	/* draw one by one */
	for (sx=0;sx < hdr->width;sx++) {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		s = data + lineoffs[sx];
		draw_vrl1_vgax_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		if ((++vga_plane) == 4) {
			if (--vramlimit == 0) break;
			vram_offset++;
			vga_plane = 0;
		}

        if ((sx+x) >= (320u-1u)) break;
	}
}

void draw_vrl1_vgax_modexclip_hflip(unsigned int x,unsigned int y,struct vrl1_vgax_header *hdr,vrl1_vgax_offset_t *lineoffs/*array hdr->width long*/,unsigned char *data,unsigned int datasz) {
#if TARGET_MSDOS == 32
	unsigned char *draw;
#else
	unsigned char far *draw;
#endif
	unsigned int vram_offset = (y * vga_state.vga_draw_stride) + (x >> 2),sx;
	unsigned int vramlimit = vga_state.vga_draw_stride_limit;
	unsigned char vga_plane = (x & 3);
	unsigned char *s;

    (void)datasz;

	/* draw one by one */
	for (sx=0;sx < hdr->width;sx++) {
		draw = vga_state.vga_graphics_ram + vram_offset;
		vga_write_sequencer(0x02/*map mask*/,1 << vga_plane);
		s = data + lineoffs[hdr->width-sx-1];
		draw_vrl1_vgax_modex_strip(draw,s);

		/* end of a vertical strip. next line? */
		if ((++vga_plane) == 4) {
			if (--vramlimit == 0) break;
			vram_offset++;
			vga_plane = 0;
		}

        if ((sx+x) >= (320u-1u)) break;
	}
}

