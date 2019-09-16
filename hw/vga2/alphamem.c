
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#ifndef TARGET_PC98

/* TODO: This API must be replaced.
 *
 *       Replace it with a general "init from video mode" that reads from the BIOS.
 *       That way one function call takes care of video pointer, number of rows and
 *       columns (from BIOS).
 *
 * TODO: Add variables for width and height, byte values. The API will deliberately
 *       use the width as the stride (bytes per line) and will not maintain a
 *       separate variable for stride. It is not this code's responsibility to
 *       provide for rendering text at an offset within the bounds of a display.
 *
 * TODO: Separate variables and source code file if the program wants to manage
 *       cursor position as well. Will have internal variables for X and Y and
 *       functions to copy to/from the BIOS data area. */

vga2_alpha_base_t vga2_alpha_base = {
#ifndef TARGET_PC98 /* PC-98 segment is always 0xA000, IBM PC can be 0xB800 or 0xB800 and even 0xA000 is possible. */
 #if TARGET_MSDOS == 32
    .memptr = NULL,
 #else
    .segptr = 0,
 #endif
#endif
    .width = 80,                /* reasonable defaults, for both IBM PC and NEC PC-98 */
    .height = 25
};

/* PC-98 notes:
 *
 * - There is a 40 column mode, but it's really just 80-column mode with every other character
 *   cell skipped and the character bitmap sent to raster with each pixel doubled. Most PC-98
 *   games will write the character twice to video memory to make sure it displays properly.
 *   The WORDs per row do not change between 40-column and 80-column mode.
 *
 *   In fact it's rare for the text layer to change columns per line at all, unless the game
 *   is willing to reprogram it (which does not happen often).
 *
 *   If you are coding for PC-98 and will be doing that, update the width yourself. When you
 *   are done, restore the default 80 columns. */

#if TARGET_MSDOS == 32
VGA2_ALPHA_PTR                  vga2_alpha_memptr = NULL;
#else
uint16_t                        vga2_alpha_segptr = 0;
#endif

/* this is a function pointer so that specialty code, such as PCjr support, can
 * provide it's own version to point at wherever the video memory is. */
void                            (*vga2_update_alpha_modeinfo)(void) = vga2_update_alpha_modeinfo_default;

void vga2_update_alpha_modeinfo_default(void) {
    if ((vga2_flags & (VGA2_FLAG_EGA|VGA2_FLAG_VGA)) != 0u) {
        /* EGA/VGA: Could be mono or color.
         * NTS: We could read it back from hardware on VGA, but that's an extra IF statement
         *      and I think it would be better for executable size reasons to combine EGA/VGA
         *      case of inferring it from BIOS data.
         *
         * NTS: One would think you'd infer this from the equipment word, but most software,
         *      including Microsoft software, is determining B800 vs B000 by reading the
         *      video mode and assuming B000 if mode 7.
         *
         *      This makes sense since on original CGA hardware mode 7 doesn't exist, and
         *      on MDA, only mode 7 exists.
         *
         *      (well actually the 5150 BIOS I have with CGA does allow mode 7 *bug*, in which
         *      case the CGA output is programmed to MDA timing!) */
        /* Another, possibly more effective technique that would be extra work here, would be
         * to use INT 10h AH=12h BL=10h Alternate Function Select (Get EGA info) to determine
         * whether the card is in mono or color mode (BH value on return). That would be more
         * useful to the alphanumeric and graphics libraries than this code. */
        vga2_alpha_ptr_set((vga2_get_int10_current_mode() == 7u) ? 0xB000u : 0xB800u);
    }
    else {
        /* CGA/MCGA: Color, always */
        /* MDA/Hercules: Mono, always */
        /* MCGA is hard-wired to map 64KB of RAM to A0000-AFFFF at all times,
         * with B0000-B7FFF unmapped and B8000-BFFFF an alias of the last 32KB. CGA and MCGA cannot respond to B000h. */
        vga2_alpha_ptr_set((vga2_flags & (VGA2_FLAG_CGA|VGA2_FLAG_MCGA)) ? 0xB800u : 0xB000u);
    }
}

#endif

