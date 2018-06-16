
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

	return 0;
}

