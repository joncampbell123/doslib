
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

int main(int argc,char **argv) {
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

    /* this code uses direct H/W access because there is no INT 10h function (that I know) to get the font data loaded into VGA RAM */

    /* what exactly this code does requires explanation:
     *
     * VGA RAM is "planar".
     *
     * This planar arrangement is most evident in the 16-color modes (640x480, 640x350, 620x200, etc)
     * where each 8-bit "plane" is combined per-bit to form 4-bit pixels that are then mapped through
     * the attribute controller and VGA palette and then sent to the DAC on the VGA connector.
     *
     * What is less obvious is that this planar arrangement is the base foundation for how ALL video
     * modes on VGA hardware work, including GGA compatible modes and alphanumeric text mode. The
     * difference is only in how the video data is mapped and how the CPU-visible "RAM" is presented.
     *
     * Theoretically, EGA follows the same design, but I have no hardware to verify this, only
     * bits and pieces of documentation. Since EGA is the base foundation for this planar memory
     * design, it's fair to assume VGA inherited the same design with enhancements and therefore
     * that EGA uses the same font RAM layout.
     *
     * This code relies on the fact that EGA/VGA hardware implement the alphanumeric text modes atop
     * the planar memory layout as follows:
     *
     * bitplane 0 = Characters, the visible text you see
     * bitplane 1 = Attributes, the colors of the text
     * bitplane 2 = VGA FONT bitmap data (256 8x32 1-bit bitmaps)
     * bitplane 3 = ?????
     *
     * For backwards compatibility with existing DOS software written around CGA/MDA hardware,
     * various bits are set in the VGA hardware, including "Odd/Even" mode, which instructs the
     * VGA hardware to present only the first two bitplanes as interleaved bytes to the CPU.
     *
     * When the CPU reads/writes a byte of video RAM, the VGA maps it to a byte offset in the
     * bitplane by masking bit 0, after using bit 0 to determine which bitplane to access.
     *
     * This code dumps VGA FONT RAM by switching the VGA hardware into a mode where bitplane 2
     * becomes accessible, so that we can read it and dump it to a file, before switching back
     * to the odd/even mode needed for CGA/MDA compatible text mode. */

    /* vga_read_GC() and vga_write_GC() are inline functions in vga.h */
    /* Graphics Controller registers are 0x3CE-0x3CF */
    /* Sequencer registers are 0x3C4-0x3C5 */

    /* FIXME: This code ASSUMES the VGA display is in text mode */

    {
        /* change VGA state to enable access to bitplane 2.
         * for best compatibility, save the old values to allow restoring later. */
		unsigned char seq4,ogc5,ogc6,seqmask;

        /* Misc Graphics Register (index 6):
         *  bit[7:4] = not defined
         *  bit[3:2] = memory map select
         *              00 = A0000-BFFFF
         *              01 = A0000-AFFFF
         *              10 = B0000-B7FFF (compatible with MDA/hercules)
         *              11 = B8000-BFFFF (compatible with CGA)
         *  bit[1:1] = Chain odd/even enable
         *  bit[0:0] = Alphanumeric mode disable (set to 1 for graphics) */
		ogc6 = vga_read_GC(6);
		vga_write_GC(6,ogc6 & (~0xFU)); /* switch off graphics, odd/even mode, move memory map to A0000 */

        /* Graphics Mode Register (index 5):
         *  bit[7:7] = not defined
         *  bit[6:6] = 256-color mode if set (shift256)
         *  bit[5:5] = Shift register interleave (when set, display hardware combines bitplanes 0 & 1
         *             and even/odd bits as a method of emulating CGA 320x200 4-color graphics modes)
         *  bit[4:4] = Host odd/even memory read addressing enable
         *  bit[3:3] = Read mode
         *              0 = Read mode 0 (byte returned is one byte from bitplane selected by Read Map Select)
         *              1 = Read mode 1 (byte returned is one bit per pixel that matches color set in Color Compare except for Dont Care
         *  bit[2:2] = not defined
         *  bit[1:0] = Write mode
         *              00 = Write mode 0 (rotate byte, logical op, mask by bitmask, mask bitplane, write to memory)
         *              01 = Write mode 1 (copy from latched to memory)
         *              10 = Write mode 2 (take low 4 bits from CPU and split across planes, to write a color)
         *              11 = Write mode 3 (VGA only, TODO) */
		ogc5 = vga_read_GC(5);
		vga_write_GC(5,ogc5 & (~0x1B)); /* switch off host odd/even, set read mode=0 write mode=0 */

        /* Sequencer Memory Mode (index 4)
         *  bit[7:4] = not defined
         *  bit[3:3] = Chain 4 enable (map CPU address bits [1:0] to select bitplane)
         *  bit[2:2] = Odd/even host memory write disable (set to DISABLE)
         *  bit[1:1] = Extended memory (enable full 256KB access)
         *  bit[0:0] = not defined */
		seq4 = vga_read_sequencer(4);
		vga_write_sequencer(4,0x06); /* switch off odd/even, switch off chain4, keep extended memory enabled */

        /* Map Mask (index 2)
         *  bit[7:4] = not defined
         *  bit[3:0] = memory plane write enable */
		seqmask = vga_read_sequencer(VGA_SC_MAP_MASK);
		vga_write_sequencer(VGA_SC_MAP_MASK,0x4); /* bit plane 2 */

        /* Read Map Select (index 4)
         *  bit[7:2] = not defined
         *  bit[1:0] = map select */
		vga_write_GC(4,0x02); /* select plane 2, where the font data is */

        /* reset the sequencer */
		vga_write_sequencer(0,0x01); /* synchronous reset */
		vga_write_sequencer(0,0x03);

        /* restore */
		vga_write_sequencer(4,seq4);
		vga_write_sequencer(0,0x01);
		vga_write_sequencer(0,0x03);
		vga_write_sequencer(VGA_SC_MAP_MASK,seqmask);
		vga_write_GC(4,0x00); /* select plane 0 */
		vga_write_GC(5,ogc5);
		vga_write_GC(6,ogc6);
    }

	return 0;
}

