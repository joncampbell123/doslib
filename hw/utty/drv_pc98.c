
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#include <hw/utty/utty.h>

#ifdef TARGET_PC98
static void utty_pc98__update_from_screen(void) {
    // TODO
}

static inline UTTY_ALPHA_CHAR _pc98_getchar(const UTTY_ALPHA_PTR ptr) {
    register UTTY_ALPHA_CHAR r;                     // UTTY_ALPHA_PTR is uint16_t therefore &[0x1000] = byte offset 0x2000
    r.f.ch = ptr[0x0000u];                          // A000:0000 character RAM
    r.f.at = ptr[0x1000u];                          // A200:0000 attribute RAM
    return r;
}

static inline void _pc98_setchar(const UTTY_ALPHA_PTR ptr,const UTTY_ALPHA_CHAR ch) {
    ptr[0x0000u] = ch.f.ch;
    ptr[0x1000u] = ch.f.at;
}

static UTTY_ALPHA_CHAR utty_pc98__getchar(utty_offset_t ofs) {
    return _pc98_getchar(_utty_ofs_to_ptr(ofs));
}

static utty_offset_t utty_pc98__setchar(utty_offset_t ofs,UTTY_ALPHA_CHAR ch) {
    UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);

    if (!vram_pc98_doublewide(ch.f.ch)) {
        _pc98_setchar(ptr,ch);
        return ofs + 2u;
    }

    _pc98_setchar(ptr+0u,ch);
    _pc98_setchar(ptr+1u,ch);
    return ofs + 4u;
}

static utty_offset_t utty_pc98__getcharblock(utty_offset_t ofs,UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { *chptr++ = _pc98_getchar(ptr++); } while ((--count) != 0u);
        return _utty_ptr_to_ofs(ptr);
    }

    return ofs;
}

static utty_offset_t utty_pc98__setcharblock(utty_offset_t ofs,const UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { _pc98_setchar(ptr++,*chptr++); } while ((--count) != 0u);
        return _utty_ptr_to_ofs(ptr);
    }

    return ofs;
}

static void utty_pc98__scroll(utty_offset_t dofs,utty_offset_t sofs,uint8_t w,uint8_t h) {
    if (dofs != sofs && w != 0u && h != 0u) {
        const int adj = ((dofs < sofs) ? ((int)utty_funcs.stride - (int)w) : ((-(int)utty_funcs.stride) - (int)w)) - 0x1000;
        UTTY_ALPHA_PTR dp = _utty_ofs_to_ptr(dofs);
        UTTY_ALPHA_PTR sp = _utty_ofs_to_ptr(sofs);
        uint8_t cw;

        if (adj < -0x1000) {
            dp += (h - 1u) * utty_funcs.stride;
            sp += (h - 1u) * utty_funcs.stride;
        }

        do {
            cw = w;
            do { *dp++ = *sp++; } while ((--cw) != 0u);

            {
                const unsigned int a = 0x1000u - w;
                dp += a; sp += a;
            }

            cw = w;
            do { *dp++ = *sp++; } while ((--cw) != 0u);

            dp += adj; sp += adj;
        } while ((--h) != 0u);
    }
}

static void utty_pc98__fill(utty_offset_t ofs,unsigned int count,UTTY_ALPHA_CHAR ch) {
    if (count != 0u) {
        UTTY_ALPHA_PTR p = _utty_ofs_to_ptr(ofs);
        do {
            p[0x0000u] = ch.f.ch;
            p[0x1000u] = ch.f.at;
            p++;
        } while ((--count) != 0u);
    }
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
    .getcharblock =                     utty_pc98__getcharblock,
    .setcharblock =                     utty_pc98__setcharblock,
    .scroll =                           utty_pc98__scroll,
    .fill =                             utty_pc98__fill
};

int utty_init_pc98(void) {
    utty_funcs = utty_funcs_pc98_init;
    utty_funcs.update_from_screen();
    if (utty_funcs.vram == NULL) return 0;
    return 1;
}
#endif

