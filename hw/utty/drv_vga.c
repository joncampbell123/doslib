
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

#ifndef TARGET_PC98
static void utty_vga__update_from_screen(void) {
    utty_funcs.vram =       vga_state.vga_alpha_ram;
    utty_funcs.w =          vga_state.vga_width;
    utty_funcs.h =          vga_state.vga_height;
    utty_funcs.stride =     vga_state.vga_stride;
}

static inline UTTY_ALPHA_CHAR _vga_getchar(const UTTY_ALPHA_PTR ptr) {
    register UTTY_ALPHA_CHAR r;
    r.raw = *ptr;
    return r;
}

static inline void _vga_setchar(const UTTY_ALPHA_PTR ptr,const UTTY_ALPHA_CHAR ch) {
    *ptr = ch.raw;
}

static UTTY_ALPHA_CHAR utty_vga__getchar(utty_offset_t ofs) {
    return _vga_getchar(_utty_ofs_to_ptr(ofs));
}

static utty_offset_t utty_vga__setchar(utty_offset_t ofs,UTTY_ALPHA_CHAR ch) {
    _vga_setchar(_utty_ofs_to_ptr(ofs),ch);
    return ofs + (utty_offset_t)2u;
}

static unsigned int utty_vga__getcharblock(utty_offset_t ofs,UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    register unsigned int ocount = count;

    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { *chptr++ = _vga_getchar(ptr++); } while ((--count) != 0u);
    }

    return ocount;
}

static unsigned int utty_vga__setcharblock(utty_offset_t ofs,const UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    register unsigned int ocount = count;

    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { _vga_setchar(ptr++,*chptr++); } while ((--count) != 0u);
    }

    return ocount;
}

static void utty_vga__scroll(utty_offset_t dofs,utty_offset_t sofs,uint8_t w,uint8_t h) {
    if (dofs != sofs && w != 0u && h != 0u) {
        const int adj = (dofs < sofs) ? ((int)utty_funcs.stride - (int)w) : ((-(int)utty_funcs.stride) - (int)w);
        UTTY_ALPHA_PTR dp = _utty_ofs_to_ptr(dofs);
        UTTY_ALPHA_PTR sp = _utty_ofs_to_ptr(sofs);
        uint8_t cw;

        if (adj < 0) {
            dp += (h - 1u) * utty_funcs.stride;
            sp += (h - 1u) * utty_funcs.stride;
        }

        do {
            cw = w;
            do { *dp++ = *sp++; } while ((--cw) != 0u);
            dp += adj; sp += adj;
        } while ((--h) != 0u);
    }
}

const struct utty_funcs_t utty_funcs_vga_init = {
    .update_from_screen =               utty_vga__update_from_screen,
    .getchar =                          utty_vga__getchar,
    .setchar =                          utty_vga__setchar,
    .getcharblock =                     utty_vga__getcharblock,
    .setcharblock =                     utty_vga__setcharblock,
    .scroll =                           utty_vga__scroll
};

int utty_init_vgalib(void) {
    utty_funcs = utty_funcs_vga_init;
    utty_funcs.update_from_screen();
    if (utty_funcs.vram == NULL) return 0;
    return 1;
}
#endif

