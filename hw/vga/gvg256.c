
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
#include <hw/8254/8254.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

#include <hw/vga/gvg256.h>

struct v320x200x256_VGA_state v320x200x256_VGA_state = {0};

void v320x200x256_VGA_update_from_CRTC_state() {
	struct vga_mode_params p;

	update_state_from_vga();
	vga_read_crtc_mode(&p);

	// TODO: If we detect Tseng ET3000/ET4000 and memory map at A0000 we could set this to 128KB
	v320x200x256_VGA_state.vram_size = 0x10000UL; /* assume 64KB. this mode support code doesn't assume the ability to do Mode X tricks */

	// NTS: On most SVGA chipsets except Tseng, only the DWORD mode of the CRTC has any use. Any other mode reveals the ugly truth
	//      as to how most SVGA chipsets implement "chained" VGA 320x200x256-color mode and the garbled VGA output is useless.
	// TODO: If we detect Tseng ET3000/ET4000 the VGA stride shift should reflect that byte & dword mode are the same
	v320x200x256_VGA_state.stride_shift = p.dword_mode ? 2 : (p.word_mode ? 1 : 0);

	v320x200x256_VGA_state.stride = v320x200x256_VGA_state.virt_width = vga_stride << v320x200x256_VGA_state.stride_shift;
	v320x200x256_VGA_state.width = p.horizontal_display_end * 4;
	v320x200x256_VGA_state.height = (p.vertical_display_end + p.max_scanline - 1) / p.max_scanline; /* <- NTS: Modern Intel chipsets however ignore the partial last scanline! */
	v320x200x256_VGA_state.virt_height = v320x200x256_VGA_state.vram_size / v320x200x256_VGA_state.stride;
	v320x200x256_VGA_state.vis_offset = p.offset << v320x200x256_VGA_state.stride_shift;
	v320x200x256_VGA_state.hpel = (vga_read_AC(0x13) >> 1) & 7;
	v320x200x256_VGA_update_draw_ptr();
	v320x200x256_VGA_update_vis_ptr();
}

