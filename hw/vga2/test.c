
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint16_t                        vga2_flags = 0;

extern uint16_t                 vga2_flags;
#define VGA2_FLAG_NONE              (0u)
#define VGA2_FLAG_MDA               (1u << 0u)
#define VGA2_FLAG_CGA               (1u << 1u)
#define VGA2_FLAG_EGA               (1u << 2u)
#define VGA2_FLAG_MCGA              (1u << 3u)
#define VGA2_FLAG_VGA               (1u << 4u)
#define VGA2_FLAG_MONO_DISPLAY      (1u << 5u)
#define VGA2_FLAG_DIGITAL_DISPLAY   (1u << 6u)
#define VGA2_FLAG_PGA               (1u << 7u)

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

uint16_t vga2_int11_equipment(void);
#pragma aux vga2_int11_equipment = \
    "int        11h" \
    value [ax] \
    modify [ax];

/* INT 10h AX=0x1A00 GET DISPLAY COMBINATION CODE (PS,VGA/MCGA) [http://www.ctyme.com/intr/rb-0219.htm].
 * Return value from BL (active display code). Does not return BH (alternate display code).
 *
 * If BL == 0xFF then the BIOS does not support the call. */
uint8_t vga2_get_dcc_inline(void);
#pragma aux vga2_get_dcc_inline = \
    "mov        ax,1A00h" \
    "xor        bx,bx" \
    "dec        bx" \
    "int        10h" \
    value [bl] \
    modify [ax bx];

uint8_t vga2_get_dcc(void) {
    return vga2_get_dcc_inline();
}

/* return the color/mono state of EGA/VGA.
 * this code assumes you have already tested the card is EGA/VGA and present. */
unsigned char vga2_alt_ega_monocolor_inline(void);
#pragma aux vga2_alt_ega_monocolor_inline = \
    "mov        ah,12h" \
    "mov        bl,10h" \
    "int        10h" \
    value [bh] \
    modify [ax bx cx];

unsigned char vga2_alt_ega_monocolor(void) {
    return vga2_alt_ega_monocolor_inline();
}

// will return 0xFF if EGA not present.
// NTS: BIOSes that pre-date the EGA will not modify registers.
//      So, call INT 10h AH=12h BL=10h with CX=FFFFh. Function returns CL.
//      If the BIOS does not recognize the call, CL=FFh on return.
//      Else it contains the EGA switch settings.
unsigned char vga2_alt_ega_switches_inline(void);
#pragma aux vga2_alt_ega_switches_inline = \
    "mov        ah,12h" \
    "mov        bl,10h" \
    "xor        cx,cx" \
    "dec        cx" \
    "int        10h" \
    value [cl] \
    modify [ax bx cx];

unsigned char vga2_alt_ega_switches(void) {
    return vga2_alt_ega_switches_inline();
}

#if TARGET_MSDOS == 32
# define VGA2_FAR
#else
# define VGA2_FAR   far
#endif

void vga2_update_alpha_ptr_default(void);

/* NTS: Not only can we treat character cells on IBM PC as 16-bit, but we can also
 *      do the same on NEC PC-98 in another library because of the 16-bit character
 *      cells (attribute in another part) */
typedef uint16_t VGA2_FAR      *VGA2_ALPHA_PTR;

VGA2_ALPHA_PTR                  vga2_alpha_mem = NULL;

/* this is a function pointer so that specialty code, such as PCjr support, can
 * provide it's own version to point at wherever the video memory is. */
void                            (*vga2_update_alpha_ptr)(void) = vga2_update_alpha_ptr_default;

static inline VGA2_ALPHA_PTR vga2_seg2ptr(const unsigned short s) {
#if TARGET_MSDOS == 32
    return (VGA2_ALPHA_PTR)((unsigned int)s << 4u);
#else
    return (VGA2_ALPHA_PTR)MK_FP(s,0);
#endif
}

void vga2_update_alpha_ptr_default(void) {
    if ((vga2_flags & (VGA2_FLAG_EGA|VGA2_FLAG_VGA)) != 0u) {
        /* EGA/VGA: Could be mono or color.
         * NTS: We could read it back from hardware on VGA, but that's an extra IF statement
         *      and I think it would be better for executable size reasons to combine EGA/VGA
         *      case of inferring it from BIOS data.
         *
         * NTS: I will make the VGA case it's own if it turns out the BIOS is not reliable
         *      on this matter even under normal usage of the hardware and BIOS.
         *
         * INT 10h AH=12h BL=10h, return value BH which is 00h = color   01h = mono */
        vga2_alpha_mem = vga2_seg2ptr((vga2_alt_ega_monocolor() & 1u) ? 0xB000u : 0xB800u);
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

    /* Then: MDA/CGA detection. Prefer CGA. */
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

int main(int argc,char **argv) {
    probe_vga2();

    printf("VGA2 flags: 0x%x\n",vga2_flags);
    if (vga2_flags & VGA2_FLAG_MDA)
        printf("  - MDA\n");
    if (vga2_flags & VGA2_FLAG_CGA)
        printf("  - CGA\n");
    if (vga2_flags & VGA2_FLAG_EGA)
        printf("  - EGA\n");
    if (vga2_flags & VGA2_FLAG_VGA)
        printf("  - VGA\n");
    if (vga2_flags & VGA2_FLAG_MCGA)
        printf("  - MCGA\n");
    if (vga2_flags & VGA2_FLAG_PGA)
        printf("  - PGA\n");
    if (vga2_flags & VGA2_FLAG_MONO_DISPLAY)
        printf("  - MONO DISPLAY\n");
    if (vga2_flags & VGA2_FLAG_DIGITAL_DISPLAY)
        printf("  - DIGITAL DISPLAY\n");

#if TARGET_MSDOS == 32
    printf("Alpha ptr: %p\n",vga2_alpha_mem);
#else
    printf("Alpha ptr: %Fp\n",vga2_alpha_mem);
#endif

    return 0;
}

