
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

typedef unsigned int            utty_offset_t;

#ifdef TARGET_PC98
/* This represents the ideal pointer type for accessing VRAM. It does not necessarily contain ALL data for the char. */
typedef uint16_t UTTY_FAR      *UTTY_ALPHA_PTR;

/* PC-98 requires two WORDs, one for the 16-bit character and one for the 16-bit attribute (even if only the lower 8 are used) */
# pragma pack(push,1)
typedef struct {
    uint16_t                    ch,at;
} UTTY_ALPHA_CHAR;
# pragma pack(pop)
#else
/* This represents the ideal pointer type for accessing VRAM. It does not necessarily contain ALL data for the char. */
typedef uint16_t UTTY_FAR      *UTTY_ALPHA_PTR;

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
    UTTY_ALPHA_CHAR         (*getchar)(utty_offset_t ofs);
    void                    (*setchar)(utty_offset_t ofs,UTTY_ALPHA_CHAR ch);
    utty_offset_t           (*getofs)(uint8_t y,uint8_t x);
};

struct utty_funcs_t         utty_funcs;

int utty_init(void) {
    return 1;
}

// COMMON
utty_offset_t utty_funcs_common_getofs(uint8_t y,uint8_t x) {
    return ((utty_offset_t)y * utty_funcs.stride) + (utty_offset_t)x;
}

#ifdef TARGET_PC98
/////////////////////////////////////////////////////////////////////////////
void utty_pc98__update_from_screen(void) {
    // TODO
}

static inline UTTY_ALPHA_CHAR _pc98_getchar(utty_offset_t ofs) {
    register UTTY_ALPHA_CHAR r;                     // UTTY_ALPHA_PTR is uint16_t therefore &[0x1000] = byte offset 0x2000
    r.ch = utty_funcs.vram[ofs        ];            // A000:0000 character RAM
    r.at = utty_funcs.vram[ofs+0x1000u];            // A200:0000 attribute RAM
    return r;
}

static inline void _pc98_setchar(const utty_offset_t ofs,const UTTY_ALPHA_CHAR ch) {
    utty_funcs.vram[ofs        ] = ch.ch;
    utty_funcs.vram[ofs+0x1000u] = ch.at;
}

UTTY_ALPHA_CHAR utty_pc98__getchar(utty_offset_t ofs) {
    return _pc98_getchar(ofs);
}

void utty_pc98__setchar(utty_offset_t ofs,UTTY_ALPHA_CHAR ch) {
    _pc98_setchar(ofs,ch);
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
    .update_from_screen =               utty_pc98__update_from_screen,
    .getchar =                          utty_pc98__getchar,
    .setchar =                          utty_pc98__setchar,
    .getofs =                           utty_funcs_common_getofs
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

static inline UTTY_ALPHA_CHAR _vga_getchar(utty_offset_t ofs) {
    return utty_funcs.vram[ofs];
}

static inline void _vga_setchar(const utty_offset_t ofs,const UTTY_ALPHA_CHAR ch) {
    utty_funcs.vram[ofs] = ch;
}

UTTY_ALPHA_CHAR utty_vga__getchar(utty_offset_t ofs) {
    return _vga_getchar(ofs);
}

void utty_vga__setchar(utty_offset_t ofs,UTTY_ALPHA_CHAR ch) {
    _vga_setchar(ofs,ch);
}

const struct utty_funcs_t utty_funcs_vga_init = {
    .update_from_screen =               utty_vga__update_from_screen,
    .getchar =                          utty_vga__getchar,
    .setchar =                          utty_vga__setchar,
    .getofs =                           utty_funcs_common_getofs
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

    {
        const char *msg = "Hello world";
        utty_offset_t o = utty_funcs.getofs(4/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uch;
        unsigned char c;

#ifdef TARGET_PC98
        uch.at = 0x0041u;       // red
#else
        uch = 0x0C00u;          // bright red
#endif

        while ((c=(*msg++)) != 0) {
#ifdef TARGET_PC98
            uch.ch = c;
#else
            uch = (uch & 0xFF00u) | c;
#endif
            utty_funcs.setchar(o++,uch);
        }
    }

    return 0;
}

