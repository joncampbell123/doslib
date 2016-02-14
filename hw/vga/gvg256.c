
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
struct vga_mode_params v320x200x256_VGA_crtc_state = {0};
struct vga_mode_params v320x200x256_VGA_crtc_state_init = {0}; // initial state after setting mode

void v320x200x256_VGA_init() {
	if (tseng_et3000_detect()) {
		v320x200x256_VGA_state.tseng = 1;
		v320x200x256_VGA_state.tseng_et4000 = 0;
	}
	else if (tseng_et4000_detect()) {
		v320x200x256_VGA_state.tseng = 1;
		v320x200x256_VGA_state.tseng_et4000 = 1;
	}
	else {
		v320x200x256_VGA_state.tseng = 0;
		v320x200x256_VGA_state.tseng_et4000 = 0;
	}
}

void v320x200x256_VGA_setmode(unsigned int flags) {
	if (!(flags & v320x200x256_VGA_setmode_FLAG_DONT_USE_INT10))
		int10_setmode(0x13);

	if (v320x200x256_VGA_state.tseng)
		vga_set_memory_map(0/*0xA0000-0xBFFFF*/);
	else
		vga_set_memory_map(1/*0xA0000-0xAFFFF*/);

	update_state_from_vga();
	v320x200x256_VGA_update_from_CRTC_state();
	v320x200x256_VGA_crtc_state_init = v320x200x256_VGA_crtc_state;
}

void v320x200x256_VGA_update_from_CRTC_state() {
	update_state_from_vga();
	vga_read_crtc_mode(&v320x200x256_VGA_crtc_state);

	if (v320x200x256_VGA_state.tseng && vga_ram_base == 0xA0000UL && vga_ram_size == 0x20000UL) {
		v320x200x256_VGA_state.vram_size = 0x20000UL; /* 128KB */
		v320x200x256_VGA_state.stride_shift = v320x200x256_VGA_crtc_state.dword_mode ? 2 : (v320x200x256_VGA_crtc_state.word_mode ? 1 : 2); /* byte/dword mode are the same */
	}
	else {
		v320x200x256_VGA_state.vram_size = 0x10000UL; /* 64KB */
		v320x200x256_VGA_state.stride_shift = v320x200x256_VGA_crtc_state.dword_mode ? 2 : (v320x200x256_VGA_crtc_state.word_mode ? 1 : 0);
	}

	v320x200x256_VGA_state.stride =
		v320x200x256_VGA_state.virt_width = vga_stride << v320x200x256_VGA_state.stride_shift;
	v320x200x256_VGA_state.width = v320x200x256_VGA_crtc_state.horizontal_display_end * 4;
	v320x200x256_VGA_state.height = (v320x200x256_VGA_crtc_state.vertical_display_end +
		v320x200x256_VGA_crtc_state.max_scanline - 1) / v320x200x256_VGA_crtc_state.max_scanline; /* <- NTS: Modern Intel chipsets however ignore the partial last scanline! */
	v320x200x256_VGA_state.virt_height = v320x200x256_VGA_state.vram_size / v320x200x256_VGA_state.stride;
	v320x200x256_VGA_state.vis_offset = v320x200x256_VGA_crtc_state.offset << v320x200x256_VGA_state.stride_shift;
	v320x200x256_VGA_state.hpel = (vga_read_AC(0x13) >> 1) & 7;
	v320x200x256_VGA_update_draw_ptr();
	v320x200x256_VGA_update_vis_ptr();
}

