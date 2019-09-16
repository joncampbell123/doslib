
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

VGA2_ALPHA_PTR                  vga2_alpha_mem = NULL;

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
        vga2_alpha_mem = vga2_seg2ptr((vga2_get_int10_current_mode() == 7u) ? 0xB000u : 0xB800u);
    }
    else if ((vga2_flags & (VGA2_FLAG_CGA|VGA2_FLAG_MCGA)) != 0u) {
        /* CGA/MCGA: Color, always */
        vga2_alpha_mem = vga2_seg2ptr(0xB800u);
    }
    else {
        /* MDA/Hercules: Mono, always */
        vga2_alpha_mem = vga2_seg2ptr(0xB000u);
    }
}

