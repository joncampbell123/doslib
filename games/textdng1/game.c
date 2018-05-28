
#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <hw/cpu/endian.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <direct.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/cpu/cpu.h>
#include <hw/vga/vga.h>
#include <hw/8237/8237.h>       /* 8237 DMA */
#include <hw/8254/8254.h>       /* 8254 timer */
#include <hw/8259/8259.h>       /* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

static unsigned char apploop = 1;

void clear_screen(void) {
    vga_clear();
    vga_moveto(0,0);
    vga_write_color(0x7);
}

void do_title_screen(void) {
    unsigned int i;
    int c;

    /* clear video RAM with background */
    for (i=0;i < (80*25);i++)
        vga_state.vga_alpha_ram[i] = 0x0100 + 176U;

    /* message */
    vga_write_color(0xF);
    vga_moveto(30,8);    vga_write("Text Dungeon 1");
    vga_moveto(22,9);    vga_write("A crappy example 2D dungeon map.");
    vga_moveto(14,10);    vga_write("(C) 2018 Hackipedia/DOSLIB/TheGreatCodeholio");

    vga_moveto(22,12);    vga_write("Hit ENTER or SPACE to continue.");
    vga_moveto(28,13);    vga_write("Hit ESC to exit to DOS.");

    do {
        c = getch();
        if (c == 27) {
            apploop = 0;
            break;
        }
        else if (c == ' ' || c == 13) {
            break;
        }
    } while (1);
}

int main(int argc,char **argv,char **envp) {
    probe_dos();
    cpu_probe();
    detect_windows();
    probe_8237();
    if (!probe_8259()) {
        printf("Cannot init 8259 PIC\n");
        return 1;
    }
    if (!probe_8254()) {
        printf("Cannot init 8254 timer\n");
        return 1;
    }
	if (!probe_vga()) { /* NTS: By "VGA" we mean any VGA, EGA, CGA, MDA, or other common graphics hardware on the PC platform
                                that acts in common ways and responds to I/O ports 3B0-3BF or 3D0-3DF as well as 3C0-3CF */
		printf("VGA probe failed\n");
		return 1;
	}

    /* this runs in text mode 80x25 */
	int10_setmode(3); /* 80x25 */
	update_state_from_vga(); /* make sure the VGA library knows so the VGA ptr values work. */
                             /* text is at either B000:0000 (mono/MDA) or B800:0000 (color/CGA) */

    while (apploop) {
        do_title_screen();
        if (!apploop) break;
    }

    clear_screen();
    vga_moveto(0,22); /* x,y near bottom of screen */
	vga_sync_bios_cursor();
    return 0;
}

