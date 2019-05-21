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

void vga_scroll_up(unsigned char lines) {
	VGA_ALPHA_PTR rd,wr;
	unsigned char row,c;

	if (lines == 0)
		return;
	else if (lines > vga_state.vga_height)
		lines = vga_state.vga_height;

	if (lines < vga_state.vga_height) {
		unsigned char lcopy = vga_state.vga_height - lines;
		wr = vga_state.vga_alpha_ram;
		rd = vga_state.vga_alpha_ram + (lines * vga_state.vga_stride);
		for (row=0;row < lcopy;row++) {
			for (c=0;c < vga_state.vga_stride;c++) {
#if defined(TARGET_PC98)
                wr[0x0000u] = rd[0x0000u];
                wr[0x1000u] = rd[0x1000u];
                wr++; rd++;
#else
				*wr++ = *rd++;
#endif
			}
		}
	}

	wr = vga_state.vga_alpha_ram + ((vga_state.vga_height - lines) * vga_state.vga_stride);
	for (row=0;row < lines;row++) {
		for (c=0;c < vga_state.vga_stride;c++) {
#if defined(TARGET_PC98)
            wr[0x0000u] = 0x20;
            wr[0x1000u] = vga_state.vga_color;
            wr++;
#else
			*wr++ = (vga_state.vga_color << 8) | 0x20;
#endif
        }
	}
}

void vga_cursor_down() {
	if (++vga_state.vga_pos_y >= vga_state.vga_height) {
		vga_state.vga_pos_y = vga_state.vga_height - 1;
		vga_scroll_up(1);
	}
}

void vga_writec(char c) {
	if (c == '\n') {
		vga_state.vga_pos_x = 0;
		vga_cursor_down();
	}
	else if (c == '\t') {
		vga_state.vga_pos_x = (vga_state.vga_pos_x | 7) + 1;
		if (vga_state.vga_pos_x >= vga_state.vga_width) {
			vga_state.vga_pos_x = 0;
			vga_cursor_down();
		}
	}
	else {
		if (vga_state.vga_pos_x >= vga_state.vga_width) {
			vga_state.vga_pos_x = 0;
			vga_cursor_down();
		}

#if defined(TARGET_PC98)
		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x] = c;
		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x + 0x1000u] = vga_state.vga_color;
#else
		vga_state.vga_alpha_ram[(vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x] = c | (vga_state.vga_color << 8);
#endif
		vga_state.vga_pos_x++;
	}
}

void vga_write(const char *msg) {
	while (*msg != 0) vga_writec(*msg++);
}

void vga_write_sync() { /* sync writing pos with BIOS cursor and hardware */
#if defined(TARGET_PC98)
		unsigned short ofs = ((vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x) * 2;
        __asm {
            push    dx
            mov     ah,0x13             ; INT 18h AH=13h set cursor position
            mov     dx,ofs
            int     18h
            pop     dx
        }
#else
	if (vga_state.vga_alpha_mode) {
		unsigned int ofs = (vga_state.vga_pos_y * vga_state.vga_stride) + vga_state.vga_pos_x;
		vga_write_CRTC(0xE,ofs >> 8);
		vga_write_CRTC(0xF,ofs);
	}
#endif
}

void vga_clear() {
	VGA_ALPHA_PTR wr;
	unsigned char r,c;

	wr = vga_state.vga_alpha_ram;
	for (r=0;r < vga_state.vga_height;r++) {
		for (c=0;c < vga_state.vga_stride;c++) {
#if defined(TARGET_PC98)
            wr[0x0000u] = 0x20;
            wr[0x1000u] = 0xE1;
            wr++;
#else
			*wr++ = 0x0720;
#endif
		}
	}
}

int vga_getch() {
    unsigned int c;

#if defined(TARGET_PC98)
    // WARNING: For extended codes this routine assumes you've set the pc98 mapping else it won't work.
    //          The mapping is reprogrammed so that function and editor keys are returned as 0x7F <code>
    //          where the <code> is assigned by us.
    //
    //          Remember that on PC-98 MS-DOS the bytes returned by function and editor keys are by
    //          default ANSI codes but can be changed via INT DCh functions, so we re-define them within
    //          the program as 0x7F <code> (same idea as VZ.EXE)
    //
    //          The INT DCh interface used for this accepts NUL-terminated strings, which means it is
    //          not possible to define IBM PC scan code like strings like 0x00 0x48, so 0x7F is used
    //          instead.
    c = (unsigned int)getch();
    if (c == 0x7Fu) c = (unsigned int)getch() << 8u;
#else
    // IBM PC: Extended codes are returned as 0x00 <scan code>, else normal ASCII
    c = (unsigned int)getch();
    if (c == 0x00u) c = (unsigned int)getch() << 8u;
#endif

    return (int)c;
}

#if defined(TARGET_PC98)
# include <hw/necpc98/necpc98.h>
void vga_tty_pc98_mapping(nec_pc98_intdc_keycode_state_ext *map) {
    unsigned int i;

    memset(map,0,sizeof(*map));

    /* function keys */
    for (i=0;i < 10;i++) {
        map->func[i].skey[0] = 0x7F;                // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->func[i].skey[1] = 0x30 + i;            // F1-F10 to 0x30-0x39
    }
    for (i=0;i < 5;i++) {
        map->vfunc[i].skey[0] = 0x7F;               // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->vfunc[i].skey[1] = 0x3A + i;           // VF1-VF10 to 0x3A-0x3E
    }
    for (i=0;i < 10;i++) {
        map->shift_func[i].skey[0] = 0x7F;          // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->shift_func[i].skey[1] = 0x40 + i;      // shift F1-F10 to 0x40-0x49
    }
    for (i=0;i < 5;i++) {
        map->shift_vfunc[i].skey[0] = 0x7F;         // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->shift_vfunc[i].skey[1] = 0x4A + i;     // shift VF1-VF10 to 0x4A-0x4E
    }
    for (i=0;i < 10;i++) {
        map->ctrl_func[i].skey[0] = 0x7F;           // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->ctrl_func[i].skey[1] = 0x50 + i;       // control F1-F10 to 0x50-0x59
    }
    for (i=0;i < 5;i++) {
        map->ctrl_vfunc[i].skey[0] = 0x7F;          // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->ctrl_vfunc[i].skey[1] = 0x5A + i;      // control VF1-VF10 to 0x5A-0x5E
    }
    for (i=0;i < 11;i++) {
        map->editkey[i].skey[0] = 0x7F;             // cannot use 0x00, that's the string terminator. VZ.EXE uses this.
        map->editkey[i].skey[1] = 0x60 + i;         // editor keys (see enum) to 0x60-0x6A
    }
}
#endif

