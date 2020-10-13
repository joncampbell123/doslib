
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
#include "fonts.h"
#include "vrlimg.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "fzlibdec.h"
#include "fataexit.h"

/*---------------------------------------------------------------------------*/
/* vga, swap current and next page pointers (not yet CRTC)                   */
/*---------------------------------------------------------------------------*/

void vga_swap_pages() {
    const unsigned p = vga_cur_page;
    vga_cur_page = vga_next_page;
    vga_next_page = p;
    vga_state.vga_graphics_ram = orig_vga_graphics_ram + vga_next_page;
}

/*---------------------------------------------------------------------------*/
/* vga, update current page to hardware (reprogram CRTC)                     */
/*---------------------------------------------------------------------------*/

void vga_update_disp_cur_page() {
    /* make sure the CRTC is not in the blanking portions or vsync
     * so that we're not changing offset during a time the CRTC might
     * latch the new display offset.
     *
     * caller is expected to wait for vsync start at some point to
     * keep time with vertical refresh or bad things (flickering)
     * happen. */
    vga_wait_for_vsync_end();
    vga_wait_for_hsync_end();
    vga_set_start_location(vga_cur_page);
}

/*---------------------------------------------------------------------------*/
/* clear next VGA page                                                       */
/*---------------------------------------------------------------------------*/

void vga_clear_npage() {
    vga_write_sequencer(0x02/*map mask*/,0xF);
    vga_rep_stosw(vga_state.vga_graphics_ram,0,0x4000u/2u); /* 16KB (8KB 16-bit WORDS) */
}

