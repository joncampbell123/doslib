
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8259/8259.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

/* FIXME: This code assumes 80x25 text */

static unsigned char editmode = 0;
static unsigned char cursx = 0,cursy = 0;
static unsigned char statrow = 25 - 1;
static unsigned char sel = 'A';
static unsigned char run = 1;

static unsigned char tmpcell[32];
static char tmp[128];

void drawstat(void) {
    VGA_ALPHA_PTR p = vga_state.vga_alpha_ram + (80 * statrow);
    unsigned int i;

    i  = 0;
    i += sprintf(tmp+i," x=%03u y=%03u %s",cursx,cursy,editmode?"Font bitmap mode":"Alpha draw mode");

    p[0] = 0x0F00 | sel;
    for (i=0;i < 79;i++) p[i+1] = 0x7000 | (unsigned char)tmp[i];
}

void clearstat(void) {
    VGA_ALPHA_PTR p = vga_state.vga_alpha_ram + (80 * statrow);
    unsigned int i;

    for (i=0;i < 80;i++) p[i] = 0x0720;
}

static unsigned char fp_seq4,fp_ogc5,fp_ogc6,fp_seqmask;

void vga_enter_fontplane(void) {
    fp_ogc6 = vga_read_GC(6);
    vga_write_GC(6,fp_ogc6 & (~0xFU)); /* switch off graphics, odd/even mode, move memory map to A0000 */

    fp_ogc5 = vga_read_GC(5);
    vga_write_GC(5,fp_ogc5 & (~0x1B)); /* switch off host odd/even, set read mode=0 write mode=0 */

    fp_seq4 = vga_read_sequencer(4);
    vga_write_sequencer(4,0x06); /* switch off odd/even, switch off chain4, keep extended memory enabled */

    fp_seqmask = vga_read_sequencer(VGA_SC_MAP_MASK);
    vga_write_sequencer(VGA_SC_MAP_MASK,0x4); /* bit plane 2 */

    vga_write_GC(4,0x02); /* select plane 2, where the font data is */
}

void vga_leave_fontplane(void) {
    /* reset the sequencer */
    vga_write_sequencer(0,0x01); /* synchronous reset */
    vga_write_sequencer(0,0x03);

    /* restore */
    vga_write_sequencer(4,fp_seq4);
    vga_write_sequencer(0,0x01);
    vga_write_sequencer(0,0x03);
    vga_write_sequencer(VGA_SC_MAP_MASK,fp_seqmask);
    vga_write_GC(4,0x00); /* select plane 0 */
    vga_write_GC(5,fp_ogc5);
    vga_write_GC(6,fp_ogc6);
}

void readcharcell(unsigned char *buf,unsigned char cell) {
    vga_enter_fontplane();

#if TARGET_MSDOS == 32
    memcpy(buf,(unsigned char*)0xA0000 + ((unsigned int)cell * 32),32);
#else
    _fmemcpy(buf,MK_FP(0xA000,((unsigned int)cell * 32)),32);
#endif

    vga_leave_fontplane();
}

void writecharcell(unsigned char *buf,unsigned char cell) {
    vga_enter_fontplane();

#if TARGET_MSDOS == 32
    memcpy((unsigned char*)0xA0000 + ((unsigned int)cell * 32),buf,32);
#else
    _fmemcpy(MK_FP(0xA000,((unsigned int)cell * 32)),buf,32);
#endif

    vga_leave_fontplane();
}

void updatecursor(void) {
    vga_moveto(cursx,cursy);
    vga_write_sync();
}

void drawchar(void) {
    unsigned int x,y,a;

    readcharcell(tmpcell,sel);

    for (y=0;y < 24;y++) {
        VGA_ALPHA_PTR p = vga_state.vga_alpha_ram + (80 * y) + 80 - (8*2);
        for (x=0;x < 8;x++,p += 2) {
            if (tmpcell[y] & (0x80 >> x))
                a = 7;
            else
                a = 1;

            if (x == cursx && y == cursy)
                a = 6;

            *((VGA_RAM_PTR)(&p[0]) + 1ul) = a << 4;
            *((VGA_RAM_PTR)(&p[1]) + 1ul) = a << 4;
        }
    }
}

void clearchar(void) {
    unsigned int x,y;

    for (y=0;y < 24;y++) {
        VGA_ALPHA_PTR p = vga_state.vga_alpha_ram + (80 * y) + 80 - (8*2);
        for (x=0;x < 8;x++,p += 2) {
            *((VGA_RAM_PTR)(&p[0]) + 1ul) = 0x07;
            *((VGA_RAM_PTR)(&p[1]) + 1ul) = 0x07;
        }
    }
}

int main(int argc,char **argv) {
    int c;

    /* probe_vga() is defined in vga.c */
	if (!probe_vga()) {
		printf("VGA probe failed\n");
		return 1;
	}

    /* this code requires EGA/VGA/SVGA.
     * I have no documentation whether or not MCGA can support this... probably not.
     * I do not have hardware to test this on EGA, but it should work. --J.C. */
	if ((vga_state.vga_flags & (VGA_IS_VGA|VGA_IS_EGA)) == 0) {
        printf("This code requires EGA, VGA, or SVGA hardware.\n");
        return 1;
    }

    while (run) {
        if (editmode) {
            unsigned char o_x = cursx;
            unsigned char o_y = cursy;

            cursx = 0;
            cursy = 0;
            updatecursor();

            drawchar();
            drawstat();
            do {
                c = getch();
                if (c == 0) c = getch() << 8;

                if (c == 27) {
                    run=0;
                    break;
                }

                if (c == 0x4800) { // IBM PC BIOS scan code UP ARROW
                    if (cursy > 0) {
                        cursy--;
                        drawchar();
                        drawstat();
                    }
                }
                else if (c == 0x5000) { // IBM PC BIOS scan code DOWN ARROW
                    if (cursy < (statrow-1u)) {
                        cursy++;
                        drawchar();
                        drawstat();
                    }
                }
                else if (c == 0x4B00) { // IBM PC BIOS scan code LEFT ARROW
                    if (cursx > 0) {
                        cursx--;
                        drawchar();
                        drawstat();
                    }
                }
                else if (c == 0x4D00) { // IBM PC BIOS scan code RIGHT ARROW
                    if (cursx < 7) {
                        cursx++;
                        drawchar();
                        drawstat();
                    }
                }
                else if (c == ' ') { // SPACEBAR
                    readcharcell(tmpcell,sel);

                    if (cursy < 32 && cursx < 8)
                        tmpcell[cursy] ^= 0x80 >> cursx;

                    writecharcell(tmpcell,sel);
                }
                else if (c == '=' || c == '+') { // =/+
                    sel++;
                    drawchar();
                    drawstat();
                }
                else if (c == '-' || c == '_') { // -/_
                    sel--;
                    drawchar();
                    drawstat();
                }
                else if (c == 'e') { // 'e'
                    editmode ^= 1;
                    break;
                }
            } while(1);

            cursx = o_x;
            cursy = o_y;
            updatecursor();
            clearchar();
        }
        else {
            updatecursor();
            drawstat();
            do {
                c = getch();
                if (c == 0) c = getch() << 8;

                if (c == 27) {
                    run=0;
                    break;
                }

                if (c == 0x4800) { // IBM PC BIOS scan code UP ARROW
                    if (cursy > 0) {
                        cursy--;
                        updatecursor();
                        drawstat();
                    }
                }
                else if (c == 0x5000) { // IBM PC BIOS scan code DOWN ARROW
                    if (cursy < (statrow-1u)) {
                        cursy++;
                        updatecursor();
                        drawstat();
                    }
                }
                else if (c == 0x4B00) { // IBM PC BIOS scan code LEFT ARROW
                    if (cursx > 0) {
                        cursx--;
                        updatecursor();
                        drawstat();
                    }
                }
                else if (c == 0x4D00) { // IBM PC BIOS scan code RIGHT ARROW
                    if (cursx < 79) {
                        cursx++;
                        updatecursor();
                        drawstat();
                    }
                }
                else if (c == ' ') { // SPACEBAR
                    /* write to the screen */
                    vga_state.vga_alpha_ram[(cursy*80)+cursx] = 0x0700 + sel;
                }
                else if (c == '=' || c == '+') { // =/+
                    sel++;
                    drawstat();
                }
                else if (c == '-' || c == '_') { // -/_
                    sel--;
                    drawstat();
                }
                else if (c == 'e') { // 'e'
                    editmode ^= 1;
                    break;
                }
            } while(1);
        }
    }
    vga_moveto(0,24);
    vga_sync_bios_cursor();
    clearstat();
	return 0;
}

