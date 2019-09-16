
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

/* INT 10h AX=0x1A00 GET DISPLAY COMBINATION CODE (PS,VGA/MCGA) values of BL [http://www.ctyme.com/intr/rb-0219.htm] */
#define probe_vga2_dcc_to_flags_sz 0x0D
static const uint8_t probe_vga2_dcc_to_flags[probe_vga2_dcc_to_flags_sz] = {
    VGA2_FLAG_NONE,                                                                         // 0x00
    VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,                     // 0x01
    VGA2_FLAG_CGA | VGA2_FLAG_DIGITAL_DISPLAY,                                              // 0x02
    VGA2_FLAG_NONE,                                                                         // 0x03 reserved
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_DIGITAL_DISPLAY,                              // 0x04
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY,     // 0x05
    VGA2_FLAG_CGA | VGA2_FLAG_PGA,                                                          // 0x06
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_VGA | VGA2_FLAG_MONO_DISPLAY,                 // 0x07
    VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_VGA,                                          // 0x08
    VGA2_FLAG_NONE,                                                                         // 0x09
    VGA2_FLAG_CGA | VGA2_FLAG_MCGA | VGA2_FLAG_DIGITAL_DISPLAY,                             // 0x0A
    VGA2_FLAG_CGA | VGA2_FLAG_MCGA | VGA2_FLAG_MONO_DISPLAY,                                // 0x0B
    VGA2_FLAG_CGA | VGA2_FLAG_MCGA                                                          // 0x0C
};

uint16_t                        vga2_flags = 0;

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

/* Unlike the first VGA library, this probe function only focuses on the primary
 * classification of video hardware: MDA, CGA, EGA, VGA, MCGA, and PGA. The code
 * is kept minimal to keep executable size down. Tandy and PCjr will show up as
 * CGA, Hercules as MDA, any SVGA chipset as VGA.
 *
 * Tandy, PCjr, SVGA, Hercules, and any other secondary classification will have
 * their own detection routines if your program is interested in them. The goal
 * is that, if a program is not interested in PCjr or Tandy support then it can
 * keep code bloat down by not calling the probe function for then and sticking
 * with the primary classification here. */
void probe_vga2(void) {
    if (vga2_flags != 0)
        return;

    /* First: VGA/MCGA detection. Ask the BIOS. */
    {
        const unsigned char dcc = vga2_get_dcc_inline();
        if (dcc < probe_vga2_dcc_to_flags_sz) {
            vga2_flags = probe_vga2_dcc_to_flags[dcc];
            goto done;
        }
    }

    /* Then: EGA. Ask the BIOS */
    {
        const unsigned char egasw = (unsigned char)vga2_alt_ega_switches_inline() & (unsigned char)0xFEu; /* filter out LSB, we don't need it */
        if (egasw != 0xFEu) { /* CL=FFh if no EGA BIOS */
            vga2_flags = VGA2_FLAG_CGA | VGA2_FLAG_EGA | VGA2_FLAG_DIGITAL_DISPLAY;
            if (egasw == 0x04 || egasw == 0x0A) vga2_flags |= VGA2_FLAG_MONO_DISPLAY; /* CL=04h/05h or CL=0Ah/CL=0Bh */
            goto done;
        }
    }

    /* Then: MDA/CGA detection. Prefer whatever the BIOS started up with. */
    /*       Please NOTE on original IBM 5150 hardware this comes from the DIP switches on the board
     *       that tells the BIOS whether MDA or CGA is installed. The design allows both MDA and CGA
     *       to be installed at the same time in which case the DIP switches tell the BIOS which one
     *       to use by default, and therefore this library as well. */
    {
        /* equipment word, bits [5:4]   3=80x25 mono  2=80x25 color  1=40x25 color  0=EGA, VGA, PGA */
        if (((unsigned char)vga2_int11_equipment() & (unsigned char)0x30u) == (unsigned char)0x30u)
            vga2_flags |= VGA2_FLAG_MDA | VGA2_FLAG_DIGITAL_DISPLAY | VGA2_FLAG_MONO_DISPLAY;
        else
            vga2_flags |= VGA2_FLAG_CGA | VGA2_FLAG_DIGITAL_DISPLAY;
    }

done:
    vga2_update_alpha_ptr();
}

