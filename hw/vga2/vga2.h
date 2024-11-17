
#ifndef __HW_VGA2_VGA2_H
#define __HW_VGA2_VGA2_H

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

/* This data type represents one whole character. It doesn't necessarily match the typedef to video RAM
 * nor does it necessarily include the attribute.
 *
 * IBM PC: Carries char and attribute
 * PC-98:  Carries char only */
typedef uint16_t                VGA2_ALPHA_CHAR;

/* attribute values */
#ifdef TARGET_PC98
/* NTS: Attribute RAM is in a separate memory location from text.
 *      Only the low 8 bits exist but it wouldn't hurt to support the full 16-bit word in the memory location */
# define VGA2_ALPHA_ATTRMASK                    (0xFFFFu)
# define VGA2_ALPHA_ATTR2W(x)                   (x)
# define VGA2_ALPHA_W2ATTR(x)                   (x)

# define VGA2_ALPHA_ATTR_NOTSECRET              (1u << 0u)
# define VGA2_ALPHA_ATTR_BLINK                  (1u << 1u)
# define VGA2_ALPHA_ATTR_REVERSE                (1u << 2u)
# define VGA2_ALPHA_ATTR_UNDERLINE              (1u << 3u)
# define VGA2_ALPHA_ATTR_VLINE                  (1u << 4u)
# define VGA2_ALPHA_ATTR_SGRAPHICS              (1u << 4u)
# define VGA2_ALPHA_ATTR_COLOR(x)               (((x) & 7u) << 5u)
#else
# define VGA2_ALPHA_ATTRMASK                    (0xFF00u)
# define VGA2_ALPHA_ATTR2W(x)                   (((x) & 0xFFu) << 8u)
# define VGA2_ALPHA_W2ATTR(x)                   (((x) >> 8u) & 0xFFu)
# define VGA2_ALPHA_ATTR_FGCOLOR(x)             (((x) & 0xFu) << 8u)
# define VGA2_ALPHA_ATTR_BGCOLOR(x)             (((x) & 0xFu) << 12u)
# define VGA2_ALPHA_ATTR_FGBGCOLOR(f,b)         (VGA2_ALPHA_ATTR_FGCOLOR(f) | VGA2_ALPHA_ATTR_BGCOLOR(b))
# define VGA2_ALPHA_ATTR_BLINK                  (0x8000u) /* only IF blink attribute enabled */
#endif

#ifdef TARGET_PC98
/* nothing yet */
 #define                        vga2_flags (0u)
#else
extern uint8_t                  vga2_flags;
#endif
#define VGA2_FLAG_NONE              (0u)

#ifdef TARGET_PC98
/* nothing yet */
#else
# define VGA2_FLAG_MDA              (1u << 0u)
# define VGA2_FLAG_CGA              (1u << 1u)
# define VGA2_FLAG_EGA              (1u << 2u)
# define VGA2_FLAG_MCGA             (1u << 3u)
# define VGA2_FLAG_VGA              (1u << 4u)
# define VGA2_FLAG_MONO_DISPLAY     (1u << 5u)
# define VGA2_FLAG_DIGITAL_DISPLAY  (1u << 6u)
# define VGA2_FLAG_PGA              (1u << 7u)

# define VGA2_FLAG_CARD_MASK        (VGA2_FLAG_MDA|VGA2_FLAG_CGA|VGA2_FLAG_EGA|VGA2_FLAG_MCGA|VGA2_FLAG_VGA|VGA2_FLAG_PGA)
#endif

extern void                     (*vga2_update_alpha_modeinfo)(void);

/* does the hardware support active display width != stride?
 * If so, it is then possible to have wider than display panning.
 * Only EGA/VGA have this capability. CGA/MDA/PCjr/Tandy/MCGA define
 * chars per row according to active display width.
 *
 * PC-98 text layer (GDC) is known to support display width != stride */
static inline uint8_t vga2_set_alpha_width_can_set_stride(void) {
#ifdef TARGET_PC98
    /* The text layer is absolutely capable of stride != display width */
    return 1u;
#else
    // EGA/VGA have offset register, therefore stride != display width.
    // Anything else (CGA/MDA/PCjr/etc) lacks the register, therefore active display width determines chars per row (stride).
    return (vga2_flags & (VGA2_FLAG_VGA|VGA2_FLAG_EGA));
#endif
}

#ifdef TARGET_PC98
// TODO
#else
// TODO
#endif

typedef struct vga2_alpha_base_t {
#ifndef TARGET_PC98 /* segment is fixed, not movable */
 #if TARGET_MSDOS == 32
    VGA2_ALPHA_PTR              memptr;
    uint32_t                    memsz;
 #else
    uint16_t                    segptr;
    uint16_t                    segsz;
 #endif
#endif
    uint8_t                     width;
    uint8_t                     height;
} vga2_alpha_base_t;

extern vga2_alpha_base_t        vga2_alpha_base;

#ifdef TARGET_PC98
/* TODO: Should be more general library functions to read the DOS kernel area! */
# if TARGET_MSDOS == 32
static inline uint8_t *vga2_dosseg_b(const unsigned int o) {
    return ((uint8_t*)(0x600u + o));
}

static inline uint16_t *vga2_dosseg_w(const unsigned int o) {
    return ((uint16_t*)(0x600u + o));
}
# else
static inline uint8_t far *vga2_dosseg_b(const unsigned int o) {
    return ((uint8_t far*)MK_FP(0x60u,o));
}

static inline uint16_t far *vga2_dosseg_w(const unsigned int o) {
    return ((uint16_t far*)MK_FP(0x60u,o));
}
# endif
#endif

/* TODO: Should be more general library functions to read the BIOS data area! */
#if TARGET_MSDOS == 32
static inline uint8_t *vga2_bda_b(const unsigned int o) {
    return ((uint8_t*)(0x400u + o));
}

static inline uint16_t *vga2_bda_w(const unsigned int o) {
    return ((uint16_t*)(0x400u + o));
}
#else
static inline uint8_t far *vga2_bda_b(const unsigned int o) {
    return ((uint8_t far*)MK_FP(0x40u,o));
}

static inline uint16_t far *vga2_bda_w(const unsigned int o) {
    return ((uint16_t far*)MK_FP(0x40u,o));
}
#endif

#ifdef TARGET_PC98
#else
static inline uint16_t vga2_bios_crtc_io(void) {
    return *vga2_bda_w(0x63);
}
#endif

static inline VGA2_ALPHA_PTR vga2_segofs2ptr(const unsigned int s,const unsigned int o) {
#if TARGET_MSDOS == 32
    return (VGA2_ALPHA_PTR)((s << 4u) + o);
#else
    return (VGA2_ALPHA_PTR)MK_FP(s,o);
#endif
}

static inline VGA2_ALPHA_PTR vga2_seg2ptr(const unsigned int s) {
    return vga2_segofs2ptr(s,0u);
}

#ifdef TARGET_PC98
/* offset from alpha text ptr to attribute word for the same character cell */
# define VGA2_PC98_ATTR_OFFSB (0x2000) /* bytes */
# define VGA2_PC98_ATTR_OFFSC (0x1000) /* chars */

static inline VGA2_ALPHA_PTR vga2_alphaofs_ptr(const unsigned int o) {
    return vga2_segofs2ptr(0xA000,o);
}

static inline void vga2_alpha_ptr_set(const unsigned int s) {
    /* nothing */
    (void)s;
}

static inline unsigned long vga2_alpha_ptrsz(void) {
    return 0x2000u; /* attribute RAM on PC-98 */
}

static inline uint16_t vga2_alpha_ptrszseg(void) {
    return 0x2000u>>4u; /* attribute RAM on PC-98 in paragraphs */
}

static inline void vga2_alpha_ptrsz_set(const unsigned long s) {
    /* nothing */
    (void)s;
}

static inline unsigned int vga2_alpha_ptr_valid(void) {
    return 1;
}
#else
 #if TARGET_MSDOS == 32
static inline VGA2_ALPHA_PTR vga2_alphaofs_ptr(const unsigned int o) {
    return (VGA2_ALPHA_PTR)((unsigned char*)vga2_alpha_base.memptr + o);
}

static inline void vga2_alpha_ptr_set(const unsigned int s) {
    vga2_alpha_base.memptr = vga2_seg2ptr(s);
}

static inline unsigned long vga2_alpha_ptrsz(void) {
    return vga2_alpha_base.memsz;
}

static inline uint16_t vga2_alpha_ptrszseg(void) {
    return (uint16_t)(vga2_alpha_base.memsz>>4u);
}

static inline void vga2_alpha_ptrsz_set(const unsigned long s) {
    vga2_alpha_base.memsz = s;
}

static inline unsigned int vga2_alpha_ptr_valid(void) {
    return vga2_alpha_base.memptr != NULL;
}
 #else
static inline VGA2_ALPHA_PTR vga2_alphaofs_ptr(const unsigned int o) {
    return vga2_segofs2ptr(vga2_alpha_base.segptr,o);
}

static inline void vga2_alpha_ptr_set(const unsigned int s) {
    vga2_alpha_base.segptr = (uint16_t)s;
}

static inline unsigned long vga2_alpha_ptrsz(void) {
    return vga2_alpha_base.segsz << 4ul;
}

static inline uint16_t vga2_alpha_ptrszseg(void) {
    return vga2_alpha_base.segsz;
}

static inline void vga2_alpha_ptrsz_set(const unsigned long s) {
    vga2_alpha_base.segsz = (uint16_t)(s >> 4ul);
}

static inline unsigned int vga2_alpha_ptr_valid(void) {
    return vga2_alpha_base.segptr != 0u;
}
 #endif
#endif

static inline VGA2_ALPHA_PTR vga2_alpha_ptr(void) {
    return vga2_alphaofs_ptr(0);
}

static inline VGA2_ALPHA_PTR vga2_alphachar_ptr(const unsigned int o) {
    return vga2_alphaofs_ptr(o * sizeof(VGA2_ALPHA_CHAR));
}

static inline VGA2_ALPHA_PTR vga2_alpharow_ptr(const unsigned int row) {
    return vga2_alphachar_ptr(row * vga2_alpha_base.width);
}

static inline VGA2_ALPHA_PTR vga2_alpharowcol_ptr(const unsigned int row,const unsigned int col) {
    return vga2_alphachar_ptr((row * vga2_alpha_base.width) + col);
}

#ifndef TARGET_PC98
/* NTS: Contrary to early impressions INT 10h AH=Fh does exist in the original 5150 BIOS. */
/*      INT 10H AH=Fh returns: AL=video mode AH=number of columns BH=active page.
 *      This function returns: AL=video mode */
uint8_t vga2_get_int10_current_mode(void);
#pragma aux vga2_get_int10_current_mode = \
    "mov        ah,0Fh" \
    "int        10h" \
    value [al] \
    modify [ax bx];
#endif

#ifndef TARGET_PC98
/* NTS: Apparently you can declare the param 8-bit, but give it to a 16-bit asm param,
 *      the compiler will zero extend for us. "MOV AX,00XXh" is fewer opcode bytes than
 *      "XOR AH,AH / MOV AL,XXh" */
void vga2_set_int10_mode(const uint8_t m);
#pragma aux vga2_set_int10_mode = \
    "int        10h" \
    modify [ax] \
    parm [ax];
#endif

#ifndef TARGET_PC98
/* NTS: start/end lines here are affected by "cursor emulation", which if enabled, the values
 *      are treated as if CGA 8-pixel high and upscaled to EGA/VGA values.
 *
 *      top_and_flags contains blink flags in bits [6:5] and top scan line bits [4:0]
 *      bottom contains bottom scan line (inside cursor) in bits [4:0] */

/* or with top_and_flags */
# define VGA2_INT10_CURSOR_SHAPE_NORMAL     (0u << 5u)
# define VGA2_INT10_CURSOR_SHAPE_INVISIBLE  (1u << 5u)
# define VGA2_INT10_CURSOR_SHAPE_ODD        (2u << 5u)  /* CGA/MDA only */
# define VGA2_INT10_CURSOR_SHAPE_SLOW       (3u << 5u)  /* CGA/MDA only */

void vga2_set_int10_cursor_shape(const uint8_t top_and_flags,const uint8_t bottom);
#pragma aux vga2_set_int10_cursor_shape = \
    "mov        ah,01h" \
    "int        10h" \
    modify [ah] \
    parm [ch] [cl];
#endif

#ifndef TARGET_PC98
/* this sets cursor position according to the BIOS's sense of chars per scanline NOT our internal representation.
 * This does NOT support multiple pages offered by the BIOS (BH=0) */
void vga2_set_int10_cursor_pos(const uint8_t row,const uint8_t col);
#pragma aux vga2_set_int10_cursor_pos = \
    "mov        ah,02h" \
    "xor        bh,bh" \
    "int        10h" \
    modify [ah bh] \
    parm [dh] [dl];
#endif

#ifndef TARGET_PC98
uint16_t vga2_int11_equipment(void);
#pragma aux vga2_int11_equipment = \
    "int        11h" \
    value [ax] \
    modify [ax];
#endif

#ifndef TARGET_PC98
/* INT 10h AX=0x1A00 GET DISPLAY COMBINATION CODE (PS,VGA/MCGA) [http://www.ctyme.com/intr/rb-0219.htm].
 * Return value from BL (active display code). Does not return BH (alternate display code).
 * BIOS RETURNS AL=1Ah to indicate function supported, but we don't check that.
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
#endif

#ifndef TARGET_PC98
/* return the color/mono state of EGA/VGA.
 * this code assumes you have already tested the card is EGA/VGA and present. */
unsigned char vga2_alt_ega_monocolor_inline(void);
#pragma aux vga2_alt_ega_monocolor_inline = \
    "mov        ah,12h" \
    "mov        bl,10h" \
    "int        10h" \
    value [bh] \
    modify [ax bx cx];
#endif

#ifndef TARGET_PC98
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
#endif

#ifndef TARGET_PC98
uint8_t vga2_get_dcc(void);
unsigned char vga2_alt_ega_monocolor(void);
unsigned char vga2_alt_ega_switches(void);
#endif

#ifdef TARGET_PC98
 #define probe_vga2() do { } while (0)
#else
void probe_vga2(void);
#endif

void vga2_update_alpha_modeinfo_default(void);

#endif //__HW_VGA2_VGA2_H

