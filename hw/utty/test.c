
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#ifdef TARGET_PC98
// TODO
#else
# include <hw/vga/vga.h>
# include <hw/vga2/vga2.h>
#endif

#include <hw/utty/utty.h>

#if TARGET_MSDOS == 32
# define UTTY_FAR
#else
# define UTTY_FAR   far
#endif

/* NTS: Not only can we treat character cells on IBM PC as 16-bit, but we can also
 *      do the same on NEC PC-98 in another library because of the 16-bit character
 *      cells (attribute in another part) */
typedef uint16_t UTTY_FAR      *UTTY_ALPHA_PTR;

#ifdef TARGET_PC98
/* PC-98 requires two WORDs, one for the 16-bit character and one for the 16-bit attribute (even if only the lower 8 are used) */
# pragma pack(push,1)
typedef struct UTTY_ALPHA_CHAR {
    uint16_t                    ch,at;
};
# pragma pack(pop)
#else
/* This data type represents one whole character. It doesn't necessarily match the typedef to video RAM. */
typedef uint16_t                UTTY_ALPHA_CHAR;
#endif

static inline UTTY_ALPHA_PTR utty_seg2ptr(const unsigned short s) {
#if TARGET_MSDOS == 32
    return (UTTY_ALPHA_PTR)((unsigned int)s << 4u);
#else
    return (UTTY_ALPHA_PTR)MK_FP(s,0);
#endif
}

struct utty_funcs_t {
    UTTY_ALPHA_PTR          vram;
    uint8_t                 w,h;
    uint16_t                stride;         // in units of type UTTY_ALPHA_PTR

    void                    (*update_from_screen)(void);
};

struct utty_funcs_t         utty_funcs;

int utty_init(void) {
    return 1;
}

#ifdef TARGET_PC98
/////////////////////////////////////////////////////////////////////////////
void utty_pc98__update_from_screen(void) {
    // TODO
}

const struct utty_funcs_t utty_funcs_pc98_init = {
#if TARGET_MSDOS == 32
    .vram =                             (UTTY_ALPHA_PTR)(0xA000u << 4u),
#else
    .vram =                             (UTTY_ALPHA_PTR)MK_FP(0xA000u,0u),
#endif
    .w =                                80,
    .h =                                25,
    .stride =                           80,
    .update_from_screen =               utty_pc98__update_from_screen
};

int utty_init_pc98(void) {
    utty_funcs = utty_funcs_pc98_init;
    utty_funcs.update_from_screen();
    if (utty_funcs.vram == NULL) return 0;
    return 1;
}
/////////////////////////////////////////////////////////////////////////////
#else
/////////////////////////////////////////////////////////////////////////////
void utty_vga__update_from_screen(void) {
    utty_funcs.vram =       vga_state.vga_alpha_ram;
    utty_funcs.w =          vga_state.vga_width;
    utty_funcs.h =          vga_state.vga_height;
    utty_funcs.stride =     vga_state.vga_stride;
}

const struct utty_funcs_t utty_funcs_vga_init = {
    .update_from_screen =               utty_vga__update_from_screen
};

int utty_init_vgalib(void) {
    utty_funcs = utty_funcs_vga_init;
    utty_funcs.update_from_screen();
    if (utty_funcs.vram == NULL) return 0;
    return 1;
}
/////////////////////////////////////////////////////////////////////////////
#endif

int main(int argc,char **argv) {
	probe_dos();

#ifdef TARGET_PC98
#else
	if (!probe_vga()) {
        printf("VGA probe failed\n");
		return 1;
	}
#endif

    if (!utty_init()) {
        printf("utty init fail\n");
        return 1;
    }
#ifdef TARGET_PC98
    if (!utty_init_pc98()) {
        printf("utty init vga fail\n");
        return 1;
    }
#else
    if (!utty_init_vgalib()) {
        printf("utty init vga fail\n");
        return 1;
    }
#endif

#if TARGET_MSDOS == 32
    printf("Alpha ptr: %p\n",utty_funcs.vram);
#else
    printf("Alpha ptr: %Fp\n",utty_funcs.vram);
#endif
    printf("Size: %u x %u (stride=%u)\n",utty_funcs.w,utty_funcs.h,utty_funcs.stride);

    return 0;
}

