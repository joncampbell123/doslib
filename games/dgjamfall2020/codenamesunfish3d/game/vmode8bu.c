
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
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

#include "timer.h"
#include "vmode.h"
#include "vrlimg.h"
#include "unicode.h"
#include "commtmp.h"
#include "fzlibdec.h"

/*---------------------------------------------------------------------------*/
/* set VGA 256-color mode unchained                                          */
/*---------------------------------------------------------------------------*/

void init_vga256unchained() {
    vga_cur_page=VGA_PAGE_FIRST;
    vga_next_page=VGA_PAGE_SECOND;

    int10_setmode(19);
    update_state_from_vga();

    /* Real hardware testing results show that the VGA BIOS will set up chained 256-color mode
     * and only clear the bytes drawn and accessible by that. If you switch on unchained mode X,
     * the junk between the chained DWORDs become visible (not cleared by VGA BIOS). So before
     * we switch to it, use the DAC mask to blank the display first, then switch on unchained
     * mode, clear video memory, then unblank the display by restoring the mask. */
    outp(0x3C6,0x00); // DAC mask: set to 0 to blank display

    vga_enable_256color_modex(); // VGA mode X (unchained)

    vga_write_sequencer(0x02/*map mask*/,0xF); // zero the video memory
    vga_rep_stosw(vga_state.vga_graphics_ram,0,0x8000u/2u);

    outp(0x3C6,0xFF); // DAC mask: restore display

    orig_vga_graphics_ram = vga_state.vga_graphics_ram;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
    vga_set_start_location(vga_cur_page);
}

