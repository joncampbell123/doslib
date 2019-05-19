
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/dos/dos.h>
#ifdef TARGET_PC98
# include <hw/necpc98/necpc98.h>
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

#define UTTY_ARRAY_LEN(x)       (sizeof(x) / sizeof(x[0]))
#define UTTY_BLANK_CHAR         { .f.ch=' ' }

typedef unsigned int            utty_offset_t;

#ifdef TARGET_PC98
# define UTTY_MAX_CHAR_WIDTH    2

/* This represents the ideal pointer type for accessing VRAM. It does not necessarily contain ALL data for the char. */
typedef uint16_t UTTY_FAR      *UTTY_ALPHA_PTR;

/* PC-98 requires two WORDs, one for the 16-bit character and one for the 16-bit attribute (even if only the lower 8 are used) */
# pragma pack(push,1)
typedef union {
    struct {
        uint16_t                    ch,at;
    } f;
    uint32_t                        raw;
} UTTY_ALPHA_CHAR;
# pragma pack(pop)
#else
# define UTTY_MAX_CHAR_WIDTH    1

/* This represents the ideal pointer type for accessing VRAM. It does not necessarily contain ALL data for the char. */
typedef uint16_t UTTY_FAR      *UTTY_ALPHA_PTR;

/* This data type represents one whole character. It doesn't necessarily match the typedef to video RAM. */
# pragma pack(push,1)
typedef union {
    struct {
        uint8_t                     ch,at;
    } f;
    uint16_t                        raw;
} UTTY_ALPHA_CHAR;
# pragma pack(pop)
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
    utty_offset_t           (*setchar)(utty_offset_t ofs,UTTY_ALPHA_CHAR ch);
    unsigned int            (*getcharblock)(utty_offset_t ofs,UTTY_ALPHA_CHAR *chptr,unsigned int count);
    unsigned int            (*setcharblock)(utty_offset_t ofs,const UTTY_ALPHA_CHAR *chptr,unsigned int count);
    utty_offset_t           (*getofs)(uint8_t y,uint8_t x);
};

struct utty_funcs_t         utty_funcs;

int utty_init(void) {
    return 1;
}

// COMMON
utty_offset_t utty_funcs_common_getofs(uint8_t y,uint8_t x) {
#if TARGET_MSDOS == 32
    const unsigned int ofs = (unsigned int)(utty_funcs.vram);
#else
    const unsigned int ofs = FP_OFF(utty_funcs.vram);
#endif
    return ((((utty_offset_t)y * utty_funcs.stride) + (utty_offset_t)x) * sizeof(uint16_t)) + (utty_offset_t)ofs;
}

static inline utty_offset_t utty_offset_advance(const utty_offset_t o,const unsigned int amt) {
    return o + (amt * 2u);
}

static inline UTTY_ALPHA_PTR _utty_ofs_to_ptr(const utty_offset_t o) {
#if TARGET_MSDOS == 32
    return (UTTY_ALPHA_PTR)(o);
#else
    return (UTTY_ALPHA_PTR)MK_FP(FP_SEG(utty_funcs.vram),(unsigned int)o);
#endif
}

#ifdef TARGET_PC98
/////////////////////////////////////////////////////////////////////////////
void utty_pc98__update_from_screen(void) {
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

UTTY_ALPHA_CHAR utty_pc98__getchar(utty_offset_t ofs) {
    return _pc98_getchar(_utty_ofs_to_ptr(ofs));
}

utty_offset_t utty_pc98__setchar(utty_offset_t ofs,UTTY_ALPHA_CHAR ch) {
    UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);

    if (!vram_pc98_doublewide(ch.f.ch)) {
        _pc98_setchar(ptr,ch);
        return ofs + 2u;
    }

    _pc98_setchar(ptr+0u,ch);
    _pc98_setchar(ptr+1u,ch);
    return ofs + 4u;
}

unsigned int utty_pc98__getcharblock(utty_offset_t ofs,UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    register unsigned int ocount = count;

    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { *chptr++ = _pc98_getchar(ptr++); } while ((--count) != 0u);
    }

    return ocount;
}

unsigned int utty_pc98__setcharblock(utty_offset_t ofs,const UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    register unsigned int ocount = count;

    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { _pc98_setchar(ptr++,*chptr++); } while ((--count) != 0u);
    }

    return ocount;
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

static inline UTTY_ALPHA_CHAR _vga_getchar(const UTTY_ALPHA_PTR ptr) {
    register UTTY_ALPHA_CHAR r;
    r.raw = *ptr;
    return r;
}

static inline void _vga_setchar(const UTTY_ALPHA_PTR ptr,const UTTY_ALPHA_CHAR ch) {
    *ptr = ch.raw;
}

UTTY_ALPHA_CHAR utty_vga__getchar(utty_offset_t ofs) {
    return _vga_getchar(_utty_ofs_to_ptr(ofs));
}

utty_offset_t utty_vga__setchar(utty_offset_t ofs,UTTY_ALPHA_CHAR ch) {
    _vga_setchar(_utty_ofs_to_ptr(ofs),ch);
    return ofs + (utty_offset_t)2u;
}

unsigned int utty_vga__getcharblock(utty_offset_t ofs,UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    register unsigned int ocount = count;

    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { *chptr++ = _vga_getchar(ptr++); } while ((--count) != 0u);
    }

    return ocount;
}

unsigned int utty_vga__setcharblock(utty_offset_t ofs,const UTTY_ALPHA_CHAR *chptr,unsigned int count) {
    register unsigned int ocount = count;

    if (count != 0u) {
        UTTY_ALPHA_PTR ptr = _utty_ofs_to_ptr(ofs);
        do { _vga_setchar(ptr++,*chptr++); } while ((--count) != 0u);
    }

    return ocount;
}

const struct utty_funcs_t utty_funcs_vga_init = {
    .update_from_screen =               utty_vga__update_from_screen,
    .getchar =                          utty_vga__getchar,
    .setchar =                          utty_vga__setchar,
    .getcharblock =                     utty_vga__getcharblock,
    .setcharblock =                     utty_vga__setcharblock,
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

unsigned int utty_string2ac(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char **msgp,UTTY_ALPHA_CHAR refch) {
#ifdef TARGET_PC98
    struct ShiftJISDecoder sjis;
    unsigned char c;
#endif
    const char *msg = *msgp;
    unsigned int r = 0;

    while (r < dst_max) {
#ifdef TARGET_PC98
        c = *msg;
        if (c == 0) break;
        msg++;

        if (pc98_sjis_dec1(&sjis,c)) {
            if ((r+2) > dst_max) break;

            c = *msg;
            if (c == 0) break;
            msg++;

            pc98_sjis_dec2(&sjis,c);

            refch.f.ch = (sjis.b1 - 0x20u) + (sjis.b2 << 8u);
            *(dst++) = refch;
            r++;
            *(dst++) = refch;
            r++;
        }
        else { 
            refch.f.ch = c;
            *(dst++) = refch;
            r++;
        }
#else
        refch.f.ch = *msg;
        if (refch.f.ch == 0) break;
        msg++;

        *(dst++) = refch;
        r++;
#endif
    }

    *msgp = msg;
    return r;
}

unsigned int utty_string2ac_const(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char *msg,UTTY_ALPHA_CHAR refch) {
    return utty_string2ac(dst,dst_max,&msg,refch);
}

int main(int argc,char **argv) {
	probe_dos();

#ifdef TARGET_PC98
	if (!probe_nec_pc98()) {
		printf("Sorry, your system is not PC-98\n");
		return 1;
	}
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
        uch.f.at = 0x0041u;         // red
#else
        uch.f.at = 0x0Cu;           // bright red
#endif

        while ((c=(*msg++)) != 0) {
            uch.f.ch = c;
            o = utty_funcs.setchar(o,uch);
        }
    }

    {
        const char *msg = "Hello world";
        utty_offset_t o = utty_funcs.getofs(5/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uach[12],uch;
        unsigned int i=0;
        unsigned char c;

#ifdef TARGET_PC98
        uch.f.at = 0x0081u;         // red
#else
        uch.f.at = 0x0Au;           // bright green
#endif

        while ((c=(*msg++)) != 0) {
            uch.f.ch = c;
            uach[i++] = uch;
        }
        assert(i < 12);
        utty_funcs.setcharblock(o,uach,i);
    }

    {
#ifdef TARGET_PC98
        const char *msg = "Hello world \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7";
#else
        const char *msg = "Hello world";
#endif
        utty_offset_t o = utty_funcs.getofs(6/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uach[40],uch = UTTY_BLANK_CHAR;
        unsigned int i=0;

#ifdef TARGET_PC98
        uch.f.at = 0x0021u;         // blue
#else
        uch.f.at = 0x09u;           // bright blue
#endif

        i = utty_string2ac_const(uach,UTTY_ARRAY_LEN(uach),msg,uch);
        assert(i < 40);
        utty_funcs.setcharblock(o,uach,i);
    }

    {
#ifdef TARGET_PC98
        const char *msg = "Hello world \x82\xb1\x82\xea\x82\xcd\x93\xfa\x96\x7b\x8c\xea\x82\xc5\x82\xb7";
#else
        const char *msg = "Hello world";
#endif
        utty_offset_t o = utty_funcs.getofs(7/*row*/,4/*col*/);
        UTTY_ALPHA_CHAR uach[UTTY_MAX_CHAR_WIDTH],uch = UTTY_BLANK_CHAR;
        const char *scan = msg,*scanfence = msg + strlen(msg);
        unsigned int i=0;

#ifdef TARGET_PC98
        uch.f.at = 0x00E1u;         // white
#else
        uch.f.at = 0x0Fu;           // white
#endif

        while (*scan != 0) {
            i = utty_string2ac(uach,UTTY_ARRAY_LEN(uach),&scan,uch);
            assert(i <= UTTY_MAX_CHAR_WIDTH);
            utty_funcs.setcharblock(o,uach,i);
            o = utty_offset_advance(o,i);
            assert(scan <= scanfence);
        }
    }

    return 0;
}

