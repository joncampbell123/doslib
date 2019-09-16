
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

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

#if TARGET_MSDOS == 32
VGA2_ALPHA_PTR                  vga2_alpha_memptr = NULL;
#else
uint16_t                        vga2_alpha_segptr = 0;
#endif

/* this is a function pointer so that specialty code, such as PCjr support, can
 * provide it's own version to point at wherever the video memory is. */
void                            (*vga2_update_alpha_ptr)(void) = vga2_update_alpha_ptr_default;

void vga2_update_alpha_ptr_default(void) {
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
        vga2_alpha_ptr_set((vga2_get_int10_current_mode() == 7u) ? 0xB000u : 0xB800u);
    }
    else if ((vga2_flags & (VGA2_FLAG_CGA|VGA2_FLAG_MCGA)) != 0u) {
        /* CGA/MCGA: Color, always */
        vga2_alpha_ptr_set(0xB800u);
    }
    else {
        /* MDA/Hercules: Mono, always */
        vga2_alpha_ptr_set(0xB000u);
    }
}

