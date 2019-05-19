
#include <stdint.h>

#if TARGET_MSDOS == 32
# define VGA2_FAR
#else
# define VGA2_FAR   far
#endif

/* NTS: Not only can we treat character cells on IBM PC as 16-bit, but we can also
 *      do the same on NEC PC-98 in another library because of the 16-bit character
 *      cells (attribute in another part) */
typedef uint16_t VGA2_FAR      *VGA2_ALPHA_PTR;

/* This data type represents one whole character. It doesn't necessarily match the typedef to video RAM. */
typedef uint16_t                VGA2_ALPHA_CHAR;

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

extern VGA2_ALPHA_PTR           vga2_alpha_mem;

extern void                     (*vga2_update_alpha_ptr)(void);

static inline VGA2_ALPHA_PTR vga2_seg2ptr(const unsigned short s) {
#if TARGET_MSDOS == 32
    return (VGA2_ALPHA_PTR)((unsigned int)s << 4u);
#else
    return (VGA2_ALPHA_PTR)MK_FP(s,0);
#endif
}

void vga2_update_alpha_ptr_default(void);

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

/* return the color/mono state of EGA/VGA.
 * this code assumes you have already tested the card is EGA/VGA and present. */
unsigned char vga2_alt_ega_monocolor_inline(void);
#pragma aux vga2_alt_ega_monocolor_inline = \
    "mov        ah,12h" \
    "mov        bl,10h" \
    "int        10h" \
    value [bh] \
    modify [ax bx cx];

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

uint8_t vga2_get_dcc(void);
unsigned char vga2_alt_ega_monocolor(void);
unsigned char vga2_alt_ega_switches(void);
void probe_vga2(void);

void vga2_update_alpha_ptr_default(void);

