
#include <i86.h>
#include <stdint.h>

#ifdef TARGET_PC98
# include <hw/necpc98/necpc98.h>
#else
# include <hw/vga/vga.h>
# include <hw/vga2/vga2.h>
#endif

#if TARGET_MSDOS == 32
# define UTTY_FAR
#else
# define UTTY_FAR   far
#endif

#define UTTY_ARRAY_LEN(x)       (sizeof(x) / sizeof(x[0]))
#define UTTY_BLANK_CHAR         { .f.ch=' ' }

typedef unsigned int            utty_offset_t;

#ifdef TARGET_PC98
/////////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////////
#else
/////////////////////////////////////////////////////////////////////////////
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
/////////////////////////////////////////////////////////////////////////////
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
};

extern struct utty_funcs_t          utty_funcs;
extern UTTY_ALPHA_CHAR              utty_tmp[16];

static inline utty_offset_t utty_offset_getofs(const uint8_t y,const uint8_t x) {
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

int utty_init(void);

unsigned int utty_string2ac(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char **msgp,UTTY_ALPHA_CHAR refch);
unsigned int utty_string2ac_const(UTTY_ALPHA_CHAR *dst,unsigned int dst_max,const char *msg,UTTY_ALPHA_CHAR refch);

utty_offset_t utty_printat(utty_offset_t o,const char **msg,UTTY_ALPHA_CHAR uch);
utty_offset_t utty_printat_const(utty_offset_t o,const char *msg,UTTY_ALPHA_CHAR uch);

#ifdef TARGET_PC98
int utty_init_pc98(void);
#endif

