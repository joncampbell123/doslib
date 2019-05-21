
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <ctype.h>
#include <i86.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/vga/vgatty.h>
#include <hw/vga/vgagui.h>

#ifdef TARGET_WINDOWS
# include <hw/dos/winfcon.h>
# include <windows/apihelp.h>
# include <windows/dispdib/dispdib.h>
# include <windows/win16eb/win16eb.h>
#endif

#if defined(TARGET_PC98)
void vga_writecw(unsigned short c) { // for use with double-wide DBCS, once decoded from Shift-JIS
    if (vram_pc98_doublewide(c)) {
		if ((vga_state.vga_pos_x+1u) >= vga_state.vga_width) {
			vga_state.vga_pos_x = 0;
			vga_cursor_down();
		}

		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x] = c;
		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x + 0x1000u] = vga_state.vga_color;
		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x + 1] = c;
		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x + 1 + 0x1000u] = vga_state.vga_color;
		vga_state.vga_pos_x += 2;
    }
    else {
        vga_writec((char)(c&0xFFu));
    }
}
#endif

#if defined(TARGET_PC98)
void vga_writew(const char *msg) { // for use with Shift-JIS DBCS
    struct ShiftJISDecoder sj;

	while (*msg != 0) {
        if (pc98_sjis_dec1(&sj,*msg++)) {
            if (*msg != 0) {
                if (pc98_sjis_dec2(&sj,*msg++))
                    vga_writecw((sj.b1 - 0x20u) + (sj.b2 << 8u));
            }
        }
        else {
            vga_writec(*msg++);
        }
    }
}
#endif

