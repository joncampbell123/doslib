
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <conio.h>

#include <hw/cpu/cpu.h>
#include <hw/dosboxid/iglib.h>

#define GDIOBJ_PEN        1    /* ;Internal */
#define GDIOBJ_BRUSH      2    /* ;Internal */
#define GDIOBJ_FONT       3    /* ;Internal */
#define GDIOBJ_PALETTE    4    /* ;Internal */
#define GDIOBJ_BITMAP     5    /* ;Internal */
#define GDIOBJ_RGN        6    /* ;Internal */
#define GDIOBJ_DC         7    /* ;Internal */
#define GDIOBJ_IC         8    /* ;Internal */
#define GDIOBJ_DISABLEDDC 9    /* ;Internal */
#define GDIOBJ_METADC    10    /* ;Internal */
#define GDIOBJ_METAFILE  11    /* ;Internal */

extern const unsigned int ScreenSelectorRaw; // hack to access "absolute" value, address of this symbol is FFFF:constant
#define ScreenSelector FP_OFF(&ScreenSelectorRaw) // Open Watcom doesn't seem to have a way to directly use KERNEL imported absolute values, so, this hack instead

unsigned char dos_version();
#pragma aux dos_version = \
    "mov    ah,0x30" \
    "int    21h" \
    modify [ah bx] \
    value [al]

unsigned int screen_width = 640;
unsigned int screen_height = 480;

/* bits per pixel */
#define vBitsPerPixel (1)
/* bytes per pixel */
#define vBytesPerPixel (0)
/* pixels per byte */
#define vPixelsPerByte (8)
/* grayscale/monochrome */
#define vGrayscale (1)
/* planar */
#define vPlanar (0)
/* maximum bytes per scanline allowed */
#define vMaxPitch (0x8000u)
/* max cursor size */
#define vMaxCursorW 32
#define vMaxCursorH 32
/* bytes per one cursor image scanline */
#define vCursorImageLineBytes ((vBytesPerPixel > 0) ? (vBytesPerPixel * vMaxCursorW) : ((((vMaxCursorW * vBitsPerPixel) + 7u) / 8u)))
/* bytes per scanline cursor buffer (cursor AND + cursor XOR + saved behind cursor + one byte) */
#define vCursorBufferPitch (((vCursorImageLineBytes) * 3u) + ((vBytesPerPixel > 0) ? vBytesPerPixel : 1u))
/* size of cursor buffer */
#define vCursorBufferSize ((vCursorBufferPitch) * (vMaxCursorH))

DWORD vga_memsize = 0;
DWORD vga_lfb = 0;
WORD vga_pitch = 0; /* bytes per scanline */
DWORD vga_visualoffset = 0; /* framebuffer offset to draw at */
DWORD vga_visualsize = 0; /* framebuffer (visible portion) size */
DWORD vga_cursorbufoffset = 0; /* framebuffer offset of cursor buffer */

unsigned int vga_drv_state = 0;
#define VGA_DRV_ENABLED (1u << 0u)

/* cursor buffer:
 *
 *   linew   linew   linew2
 * +-------+-------+---------+
 * |  AND  |  XOR  |  SAVED  |
 * |  IMG  |  IMG  |   IMG   |
 * +-------+-------+---------+
 *
 * linew2 = linew + vBytesPerPixel, saved image is vMaxCursorW + 1 pixels wide if vBytesPerPixel >= 1
 * linew2 = linew + 1u if not, saved image is vMaxCursorW + vPixelsPerByte pixels wide */

void DEBUG_OUT(const char *str) {
    unsigned char c;

    dosbox_id_write_regsel(DOSBOX_ID_REG_DEBUG_OUT);
    dosbox_id_reset_latch();
    while ((c=((unsigned char)(*str++))) != 0U) dosbox_id_write_data_nrl_u8(c);
    dosbox_id_flush_write();
}

static char debug_tmp[128],*debug_tw;
#define debug_tmp_LAST (debug_tmp + sizeof(debug_tmp) - 1)

static const char *debug_hexes = "0123456789ABCDEF";

void _setmem(void *d,const unsigned char c,unsigned int len) {
    __asm {
        push    di
        push    cx
	mov	al,c
        mov     cx,len
        les     di,d
        cld
        rep	stosb
        pop     cx
        pop     di
    }
}

void _copymem(void *d,const void *s,unsigned int len) {
    __asm {
        push    ds
        push    si
        push    di
        push    cx
        mov     cx,len
        lds     si,s
        les     di,d
        cld
        rep	movsb
        pop     cx
        pop     di
        pop     si
        pop     ds
    }
}

static unsigned long uint_mul_16x16to32(const unsigned int a,const unsigned int b);
#pragma aux uint_mul_16x16to32 = \
    "xor dx,dx" \
    "mul cx" \
    parm [ax] [cx] \
    modify [dx ax] \
    value [dx ax];

static unsigned int DEBUG_uldiv(unsigned long *v,const unsigned int d);
/* long division.
 * H = upper 16 bits of v
 * L = lower 16 bits of v
 *
 *     hq  lq         remainder lr
 *   ________
 * d ) H   L
 *                    hq = H / D
 *                    hr = H % D
 *     hr  L          next, work with 32-bit value (hr:L), which is (16-bit hr:16-bit L) combined into 32 bits
 *                    lq = (hr:L) / D
 *                    lr = (hr:L) % D
 *     hr  lr
 *
 * On x86 processors, in 16-bit mode, DIV divides DX:AX by whatever memory or CPU register you specify,
 * and if the result is small enough to fit into 16 bits, DX is the remainder and AX is the quotient.
 * At each DIV, the upper 16 bits (DX) used in the divide is the remainder from the previous DIV.
 * Remainder is returned from DX by the function.
 */
#pragma aux DEBUG_uldiv = \
    "xor dx,dx" \
     \
    "mov ax,WORD PTR es:[si+2]" \
    "div cx" \
    "mov WORD PTR es:[si+2],ax" \
    \
    "mov ax,WORD PTR es:[si]" \
    "div cx" \
    "mov WORD PTR es:[si],ax" \
    parm [es si] [cx] \
    modify [dx ax] \
    value [dx];

static void DEBUG_OUTstr(const char *str) {
    unsigned char c;

    while ((c=((unsigned char)(*str++))) != 0U) dosbox_id_write_data_nrl_u8(c);
}

static void DEBUG_OUTuint(unsigned int v) { /* set debug_tw = debug_tmp_LAST, *debug_tmp = 0, then call */
    *(--debug_tw) = (char)(v % 10u) + '0'; v /= 10u;
    while (v != 0u) { *(--debug_tw) = (char)(v % 10u) + '0'; v /= 10u; }
}

static void DEBUG_OUTulint(unsigned long v) { /* set debug_tw = debug_tmp_LAST, *debug_tmp = 0, then call */
    *(--debug_tw) = (char)DEBUG_uldiv(&v,10u) + '0';
    while (v != 0u) { *(--debug_tw) = (char)DEBUG_uldiv(&v,10u) + '0'; }
}

static void DEBUG_OUTuhex(unsigned int v) { /* set debug_tw = debug_tmp_LAST, *debug_tmp = 0, then call */
    *(--debug_tw) = debug_hexes[v&0xFu]; v >>= 4u;
    while (v != 0u) { *(--debug_tw) = debug_hexes[v&0xFu]; v >>= 4u; }
}

static void DEBUG_OUTulhex(unsigned long v) { /* set debug_tw = debug_tmp_LAST, *debug_tmp = 0, then call */
    *(--debug_tw) = debug_hexes[v&0xFu]; v >>= 4ul;
    while (v != 0ul) { *(--debug_tw) = debug_hexes[v&0xFu]; v >>= 4ul; }
}

void DEBUG_OUTF(const char *fmt,...) {
    unsigned char c;
    va_list va;

    va_start(va,fmt);
    dosbox_id_write_regsel(DOSBOX_ID_REG_DEBUG_OUT);
    dosbox_id_reset_latch();

    while ((c=((unsigned char)(*fmt++))) != 0U) {
        if (c == '%') {
            if ((c=((unsigned char)(*fmt++))) == 0)
                break;

            switch (c) {
                case 'l': {
                    if ((c=((unsigned char)(*fmt++))) == 0)
                        break;
                    switch (c) {
                        case 'd': {
                            long v = va_arg(va,long);
                            if (v < 0) {
                                v = -v;
                                dosbox_id_write_data_nrl_u8('-');
                            }
                            debug_tw = debug_tmp_LAST; *debug_tw = 0; DEBUG_OUTulint((unsigned long)v);
                            DEBUG_OUTstr(debug_tw);
                            break; }
                        case 'u': {
                            debug_tw = debug_tmp_LAST; *debug_tw = 0; DEBUG_OUTulint(va_arg(va,unsigned long));
                            DEBUG_OUTstr(debug_tw);
                            break; }
                        case 'x': {
                            debug_tw = debug_tmp_LAST; *debug_tw = 0; DEBUG_OUTulhex(va_arg(va,unsigned long));
                            DEBUG_OUTstr(debug_tw);
                            break; }
                        default:
                            break;
                    }
                    break; }
                case 'd': {
                    int v = va_arg(va,int);
                    if (v < 0) {
                        v = -v;
                        dosbox_id_write_data_nrl_u8('-');
                    }
                    debug_tw = debug_tmp_LAST; *debug_tw = 0; DEBUG_OUTuint((unsigned int)v);
                    DEBUG_OUTstr(debug_tw);
                    break; }
                case 'u': {
                    debug_tw = debug_tmp_LAST; *debug_tw = 0; DEBUG_OUTuint(va_arg(va,unsigned int));
                    DEBUG_OUTstr(debug_tw);
                    break; }
                case 'x': {
                    debug_tw = debug_tmp_LAST; *debug_tw = 0; DEBUG_OUTuhex(va_arg(va,unsigned int));
                    DEBUG_OUTstr(debug_tw);
                    break; }
                default:
                    break;
            }
        }
        else {
            dosbox_id_write_data_nrl_u8(c);
        }
    }

    dosbox_id_flush_write();
    va_end(va);
}

typedef struct tagGDIINFO {
    short  int                dpVersion;
    short  int                dpTechnology;
    short  int                dpHorzSize;
    short  int                dpVertSize;
    short  int                dpHorzRes;
    short  int                dpVertRes;
    short  int                dpBitsPixel;
    short  int                dpPlanes;
    short  int                dpNumBrushes;
    short  int                dpNumPens;
    short  int                futureuse;
    short  int                dpNumFonts;
    short  int                dpNumColors;

    unsigned  short  int      dpDEVICEsize;
    unsigned  short  int      dpCurves;
    unsigned  short  int      dpLines;
    unsigned  short  int      dpPolygonals;
    unsigned  short  int      dpText;
    unsigned  short  int      dpClip;
    unsigned  short  int      dpRaster;
    short  int                dpAspectX;
    short  int                dpAspectY;
    short  int                dpAspectXY;
    short  int                dpStyleLen;
    POINT                     dpMLoWin;
    POINT                     dpMLoVpt;
    POINT                     dpMHiWin;
    POINT                     dpMHiVpt;
    POINT                     dpELoWin;

    POINT                     dpELoVpt;
    POINT                     dpEHiWin;
    POINT                     dpEHiVpt;
    POINT                     dpTwpWin;
    POINT                     dpTwpVpt;
    short  int                dpLogPixelsX;
    short  int                dpLogPixelsY;
    short  int                dpDCManage;
    short  int                dpCaps1;
    long   int                dpSpotSizeX;
    long   int                dpSpotSizeY;
    short  int                dpPalColors;
    short  int                dpPalReserved;
    short  int                dpPalResolution;
} GDIINFO;
typedef GDIINFO FAR *LPGDIINFO;

#define     InquireInfo     0x01        /* Inquire Device GDI Info         */
#define     EnableDevice    0x00        /* Enable Device                   */
#define     InfoContext     0x8000      /* Inquire/Enable for info context */

#define     TC_NONE         0x0000
#define     DC_IgnoreDFNP   0x0004

#define     CP_RECTANGLE    0x0001

#define     X_MAJOR_DIST    36
#define     Y_MAJOR_DIST    36
#define     HYPOTENUSE      51
#define     X_MINOR_DIST    HYPOTENUSE-X_MAJOR_DIST
#define     Y_MINOR_DIST    HYPOTENUSE-Y_MAJOR_DIST
#define     MAX_STYLE_ERR   HYPOTENUSE*2

struct int_phys_device {
    /* also a BITMAP, also a PDEVICE. */
    /* first word bmType if BITMAP, pdType if PDEVICE.
     * bmType == 0 if a bitmap, PDEVICE if not.
     * PDEVICE is a generic base struct from which we define all our driver-specific state,
     * therefore, if nonzero, it's our int_phys_device struct. */
    BITMAP                    bitmap;
};

GDIINFO GDIInfo = {
    .dpVersion = 0x30A, /* Windows 3.10 */
    .dpTechnology = DT_RASDISPLAY,
    .dpHorzSize = 208, /* in mm */
    .dpVertSize = 156, /* in mm */
    .dpHorzRes = 640, /* in pixels */
    .dpVertRes = 480, /* in pixels */
    .dpBitsPixel = vBitsPerPixel,
    .dpPlanes = (vPlanar > 0) ? 4 : 1,
    .dpNumBrushes = 65 + 6 + 6,
    .dpNumPens = 10, /* 2 colors * 5 styles */
    .futureuse = 0,
    .dpNumFonts = 0,
    .dpNumColors = (vBitsPerPixel <= 256) ? (1u << vBitsPerPixel) : 0,
    .dpDEVICEsize = sizeof(struct int_phys_device),
    .dpCurves = CC_NONE,
    .dpLines = LC_POLYLINE+LC_STYLED,
    .dpPolygonals = PC_SCANLINE,
    .dpText = TC_CP_STROKE+TC_RA_ABLE,
    .dpClip = CP_RECTANGLE,
    .dpRaster = RC_BITBLT+RC_BITMAP64+RC_GDI20_OUTPUT+RC_DI_BITMAP+RC_DIBTODEV,
    .dpAspectX = X_MAJOR_DIST,
    .dpAspectY = Y_MAJOR_DIST,
    .dpAspectXY = HYPOTENUSE,
    .dpStyleLen = MAX_STYLE_ERR,
    .dpMLoWin = {2080, 1560},
    .dpMLoVpt = {640, -480},
    .dpMHiWin = {20800, 15600},
    .dpMHiVpt = {640, -480},
    .dpELoWin = {325, 325},
    .dpELoVpt = {254, -254},
    .dpEHiWin = {1625, 1625},
    .dpEHiVpt = {127, -127},
    .dpTwpWin = {2340, 2340},
    .dpTwpVpt = {127, -127},
    .dpLogPixelsX = 96,
    .dpLogPixelsY = 96,
    .dpDCManage = DC_IgnoreDFNP,
    .dpCaps1 = 0,
    .dpSpotSizeX = 0,
    .dpSpotSizeY = 0,
    .dpPalColors = 0,
    .dpPalReserved = 0,
    .dpPalResolution = 0
};

int GetSettingInt(const char *name,int defval) {
    int v = -666;

#if WINVER >= 0x300
    if (v == -666) v = GetPrivateProfileInt("dosboxig.drv",name,-666,"system.ini");
#endif
    if (v == -666) v = GetProfileInt("dosboxig.drv",name,-666);
    if (v == -666) v = defval;

    return v;
}

int init_dosbox_ig(void) {
    uint32_t r;

    {
        int v;

        v = GetSettingInt("width",-1);
        if (v >= 64 && v <= 4096) screen_width = (unsigned int)v;

        v = GetSettingInt("height",-1);
        if (v >= 64 && v <= 4096) screen_height = (unsigned int)v;

        vga_pitch = (vBytesPerPixel > 0u) ? (vBytesPerPixel * screen_width) : (((vBitsPerPixel * screen_width) + 7u) / 8u);
        vga_visualsize = uint_mul_16x16to32(vga_pitch,screen_height);
        vga_visualoffset = 256ul * 1024ul; // start at 256KB so VGA memory is preserved, grabbers not necessary
    }

    DEBUG_OUTF("Screen configuration: %u bits/pixel, %u bytes/pixel, %u pixels/byte, planar=%u grayscale/mono=%u, maxpitch=%u, maxcursor=%u x %u\n",
        vBitsPerPixel,vBytesPerPixel,vPixelsPerByte,vPlanar,vGrayscale,vMaxPitch,vMaxCursorW,vMaxCursorH);
    DEBUG_OUTF("Screen configuration: %u x %u, %u bytes/line, %lu bytes visible framebuffer at offset 0x%lx(%lu), cursorbufsize=0x%lx(%lu)\n",
        screen_width,screen_height,vga_pitch,vga_visualsize,vga_visualoffset,vga_visualoffset,
        (unsigned long)vCursorBufferSize,(unsigned long)vCursorBufferSize);
    DEBUG_OUTF("cursor pitch=%u imgw=%u\n",
        vCursorBufferPitch,vCursorImageLineBytes);

    if (!vga_pitch || !vga_visualsize) return 0;

    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_CAPS);
    r = dosbox_id_read_data();
    if (!(r & DOSBOX_ID_REG_VGAIG_CAPS_ENABLED)) {
        DEBUG_OUT("Checking DOSBox IG device\n");
        return 0;
    }

    dosbox_id_write_regsel(DOSBOX_ID_REG_GET_VGA_MEMBASE);
    vga_lfb = dosbox_id_read_data();

    dosbox_id_write_regsel(DOSBOX_ID_REG_GET_VGA_MEMSIZE);
    vga_memsize = dosbox_id_read_data();

    DEBUG_OUTF("VGA lfb=0x%lx memsize=0x%lx(%lu)\n",
        (unsigned long)vga_lfb,(unsigned long)vga_memsize,(unsigned long)vga_memsize);

    if (vCursorBufferSize > vga_memsize) {
        DEBUG_OUT("Insufficient video RAM (cursor)\n");
        return 0;
    }
    vga_memsize -= vCursorBufferSize;
    vga_cursorbufoffset = vga_memsize;

    DEBUG_OUTF("VGA cursorbufoffset=0x%lx\n",(unsigned long long)vga_cursorbufoffset);

    if ((vga_visualoffset+vga_visualsize) > vga_memsize) {
        DEBUG_OUT("Insufficient video RAM (framebuffer)\n");
        return 0;
    }

    DEBUG_OUTF("ScreenSelector 0x%x\n",ScreenSelector);
    DEBUG_OUTF("Updating GDIINFO\n");
    GDIInfo.dpHorzRes = 640, /* in pixels */
    GDIInfo.dpVertRes = 480, /* in pixels */
    GDIInfo.dpMLoVpt.x = 640;
    GDIInfo.dpMLoVpt.y = -480;
    GDIInfo.dpMHiVpt.x = 640;
    GDIInfo.dpMHiVpt.y = -480;
    return 1;
}

WORD MiniLibMain(void) {
    if (!probe_dosbox_id())
        return 0;
    if (dos_version() < 3)
        return 0;

    DEBUG_OUT("DOSBox IG display driver init\n");

    if (!init_dosbox_ig())
        return 0;

    return 1;
}

/* NTS: Nowhere does Microsoft document what a PCOLOR is, so this is a best guess */
typedef DWORD PCOLOR;
typedef PCOLOR FAR *LPPCOLOR;

#define FP8(x) ((unsigned int)((x) * 255.0))

static inline BYTE RGBtoGray8(BYTE r,BYTE g,BYTE b) {
    return (BYTE)(((r * FP8(0.299)) + (g * FP8(0.587)) + (b * FP8(0.114))) >> 8u);
}

static const unsigned char Quant8ToBitsBias[8] = {
    85,  // 1bpp: 33% black 66% white just like sample driver
    32,  // 2bpp (pc * 64)
    16,  // 3bpp (pc * 32)
    8,   // 4bpp (pc * 16)
    4,   // 5bpp (pc * 8)
    2,   // 6bpp (pc * 4)
    1,   // 7bpp (pc * 2)
    0    // 8bpp
};

static inline BYTE Quant8ToBits(BYTE v,BYTE bits) {
    const unsigned int r = ((unsigned int)v + (unsigned int)Quant8ToBitsBias[bits - 1u]) >> (8u - bits);
    if (r & (0x100 >> (8u - bits)))
        return 0xFFu >> (8u - bits);
    else
        return r;
}

DWORD colorRGBtoScreen(DWORD rgb) {
#if vGrayscale > 0
    return Quant8ToBits(RGBtoGray8(rgb>>16ul,rgb>>8ul,rgb),vBitsPerPixel);
#else
    return 0;
#endif
}

DWORD colorScreenToRGB(DWORD pc) {
#if vGrayscale > 0
# if vBitsPerPixel < 8
    if (pc != 0) {
        BYTE r = 0,cn = 8;
        // move bits to top i.e. 0000xxxx -> xxxx0000 and then iteratively make the bit pattern repeat every vBitsPerPixel
        // for example,
        // 1bpp 0 -> 00000000
        // 1bpp 1 -> 11111111
        // 2bpp 10 -> 10101010
        // 2bpp 01 -> 01010101
        // 4bpp 0110 -> 01100110
        //
        // doing this is equivalent to pc = (pc * 255) / ((1 << vBitsPerPixel) - 1)
        pc <<= 8u - vBitsPerPixel;
        while (cn >= vBitsPerPixel) {
            r |= pc;
            cn -= vBitsPerPixel;
            pc >>= vBitsPerPixel;
        }
        if (cn != 0u) {
            r |= pc;
        }
        pc = r;
    }
# endif
    return pc | (pc << 8ul) | (pc << 16ul);
#else
    return 0;
#endif
}

COLORREF WINAPI my_ColorInfo(LPVOID lpDestDev,DWORD dwColorin,LPPCOLOR lpPColor) {
    (void)lpDestDev;

    if (lpPColor) {
        const DWORD pc = colorRGBtoScreen(dwColorin);
        *lpPColor = colorScreenToRGB(pc);
        return pc;
    }
    else {
        return colorScreenToRGB(dwColorin);
    }
}

void WINAPI my_Control(void) {
    __asm int 3
    __asm mov ax,3
}

void WINAPI my_Disable(LPVOID lpDestDev) {
    (void)lpDestDev;

    //__asm int 3
    if (vga_drv_state & VGA_DRV_ENABLED) {
        /* clear CTL, so changes are not effective yet */
        dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_CTL);
        dosbox_id_write_data(0);

        vga_drv_state &= ~VGA_DRV_ENABLED;
    }
}

WORD displayEnable(struct int_phys_device *lpDevInfo) {
    (void*)lpDevInfo;

    /* clear CTL, so changes are not effective yet */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_CTL);
    dosbox_id_write_data(0);

    /* set format and bytes per line */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_FMT_BYTESPERSCANLINE);
    dosbox_id_write_data(DOSBOX_ID_REG_VGAIG_FMT_1BPP | vga_pitch);

    /* screen dimensions */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_DISPLAYSIZE);
    dosbox_id_write_data(((unsigned long)screen_height << 16ul) + screen_width);

    /* h/v total add */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_HVTOTALADD);
    dosbox_id_write_data((64ul << 16ul) + 64ul);

    /* refresh rate */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_REFRESHRATE);
    dosbox_id_write_data(70ul << 16ul); /* 16.16 fixed point 70Hz */

    /* pixel scale */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_HVPELSCALE);
    dosbox_id_write_data(0);

    /* aspect ratio */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_ASPECTRATIO);
    dosbox_id_write_data(0); /* square pixels */

    /* DOSBox IG bypasses the attribute controller, program the palette */
#if vBitsPerPixel <= 8
    outp(0x3C8,0);
    outp(0x3C9,0x00); outp(0x3C9,0x00); outp(0x3C9,0x00);
    outp(0x3C9,0xFF); outp(0x3C9,0xFF); outp(0x3C9,0xFF);
#endif

    /* switch on IG */
    dosbox_id_write_regsel(DOSBOX_ID_REG_VGAIG_CTL);
    dosbox_id_write_data(DOSBOX_ID_REG_VGAIG_CTL_OVERRIDE|DOSBOX_ID_REG_VGAIG_CTL_VGAREG_LOCKOUT|DOSBOX_ID_REG_VGAIG_CTL_OVERRIDE_REFRESH|DOSBOX_ID_REG_VGAIG_CTL_ACPAL_BYPASS|DOSBOX_ID_REG_VGAIG_CTL_VBEMODESET_DISABLE|DOSBOX_ID_REG_VGAIG_CTL_A0000_FORCE);

    vga_drv_state |= VGA_DRV_ENABLED;
    return 1;
}

WORD WINAPI my_Enable(LPVOID lpDevInfo, WORD wStyle, LPSTR lpDestDevType, LPSTR lpOutputFile, LPVOID lpData) {
    (void)lpDestDevType;
    (void)lpOutputFile;
    (void)lpData;

    if (wStyle & InquireInfo) {
        _copymem((LPGDIINFO*)lpDevInfo,&GDIInfo,sizeof(GDIINFO));
        return sizeof(GDIINFO);
    }
    else if (wStyle & InfoContext) {
        return 0; // nope
    }

    return displayEnable((struct int_phys_device*)lpDevInfo);
}

void WINAPI my_EnumDFonts(void) {
    __asm int 3
    __asm mov ax,6
}

void WINAPI my_EnumObj(void) {
    __asm int 3
    __asm mov ax,7
}

void WINAPI my_Output(void) {
    __asm int 3
    __asm mov ax,8
}

void WINAPI my_Pixel(void) {
    __asm int 3
    __asm mov ax,9
}

typedef signed short SHORT;
typedef SHORT FAR *LPSHORT;

#pragma pack(push,1)
typedef struct tagPDEVICE {
    short pdType;
} PDEVICE;
typedef PDEVICE FAR *LPPDEVICE;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct tagDRAWMODE {
  short    Rop2;       /*binary-raster operations*/
  short    bkMode;     /*background mode*/
  PCOLOR   bkColor;    /*physical background color*/
  PCOLOR   TextColor;  /*physical text (foreground) color*/
  short    TBreakExtra;/*number of extra pixels to add to line*/
  short    BreakExtra; /*pixels per break: TBreakExtra/BreakCount*/
  short    BreakErr;   /*running error term*/
  short    BreakRem;   /*remaining pixels: TBreakExtra%BreakCount*/
  short    BreakCount; /*number of breaks in the line*/
  short    CharExtra;  /*extra pixels for each character*/
  COLORREF LbkColor;   /*logical background color*/

  COLORREF LTextColor; /*logical text (foreground) color*/
} DRAWMODE;
typedef DRAWMODE FAR *LPDRAWMODE;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct tagFONTINFO {
    short dfType;
    short dfPoints;
    short dfVertRes;
    short dfHorizRes;
    short dfAscent;
    short dfInternalLeading;
    short dfExternalLeading;
    char  dfItalic;
    char  dfUnderline;
    char  dfStrikeOut;
    short dfWeight;
    char  dfCharSet;
    short dfPixWidth;
    short dfPixHeight;
    char  dfPitchAndFamily;
    short dfAvgWidth;
    short dfMaxWidth;
    char  dfFirstChar;
    char  dfLastChar;
    char  dfDefaultChar;
    char  dfBreakChar;

    short dfWidthBytes;
    long  dfDevice;
    long  dfFace;
    long  dfBitsPointer;
    long  dfBitsOffset;
    char  dfReserved;
    /* The following fields present only for Windows 3.x fonts */
    long  dfFlags;
    short dfAspace;
    short dfBspace;
    short dfCspace;
    long  dfColorPointer;
    long  dfReserved1[4];
} FONTINFO;
typedef FONTINFO FAR *LPFONTINFO;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct tagTEXTXFORM {
    short  txfHeight;
    short  txfWidth;
    short  txfEscapement;
    short  txfOrientation;
    short  txfWeight;
    char   txfItalic;
    char   txfUnderline;
    char   txfStrikeOut;
    char   txfOutPrecision;
    char   txfClipPrecision;
    short  txfAccelerator;
    short  txfOverhang;
} TEXTXFORM;
typedef TEXTXFORM FAR *LPTEXTXFORM;
#pragma pack(pop)

typedef uint32_t phys_color;

#pragma pack(push,1)
struct oem_pen_def {
    phys_color          color;
    uint16_t            style;
};
typedef struct oem_pen_def *Poem_pen_def;
typedef struct oem_pen_def FAR *LPoem_pen_def;
#pragma pack(pop)

#pragma pack(push,1)
struct oem_brush_def {
    uint8_t             mono[8]; /* 8x8 */
    uint16_t            style;
    uint8_t             accel;
    phys_color          fg,bg;
    uint8_t             spare;
    uint8_t             trans[8]; /* 8x8 */
};
typedef struct oem_brush_def *Poem_brush_def;
typedef struct oem_brush_def FAR *LPoem_brush_def;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct tagLPEN {
    long  lopnStyle;
    POINT lopnWidth;
    long  lopnColor;
} LPEN;
typedef LPEN *PLPEN;
typedef LPEN FAR *LPLPEN;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct tagLBRUSH {
    short  lbStyle;
    long   lbColor;
    short  lbHatch;
    long   lbBkColor;
} LBRUSH;
typedef LBRUSH *PLBRUSH;
typedef LBRUSH FAR *LPLBRUSH;
#pragma pack(pop)

typedef DWORD (WINAPI *robj_call_t)(LPVOID lpDestDev, LPVOID lpInObj, LPVOID lpOutObj, LPTEXTXFORM lpTextXForm);

static DWORD WINAPI my_realize_pen(LPVOID lpDestDev, LPVOID lpInObj, LPVOID lpOutObj, LPTEXTXFORM lpTextXForm) {
    LPoem_pen_def o_pn = (LPoem_pen_def)lpOutObj;
    LPLPEN i_pn = (LPLPEN)lpInObj;

    _setmem(o_pn,0,sizeof(*o_pn));
    o_pn->style = i_pn->lopnStyle;
    o_pn->color = i_pn->lopnColor;

    return 1;
}

static DWORD WINAPI my_realize_brush(LPVOID lpDestDev, LPVOID lpInObj, LPVOID lpOutObj, LPTEXTXFORM lpTextXForm) {
    LPoem_brush_def o_br = (LPoem_brush_def)lpOutObj;
    LPLBRUSH i_br = (LPLBRUSH)lpInObj;

    _setmem(o_br,0,sizeof(*o_br));
    o_br->style = i_br->lbStyle;
    o_br->fg = i_br->lbColor;
    o_br->bg = i_br->lbBkColor;
    o_br->accel = 0;
    o_br->spare = 0;

    return 1;
}

static DWORD WINAPI my_realize_font(LPVOID lpDestDev, LPVOID lpInObj, LPVOID lpOutObj, LPTEXTXFORM lpTextXForm) {
    return 0; /* return 0 so GDI does it for us */
}

static const unsigned char robj_sizes[GDIOBJ_FONT] = {
    sizeof(struct oem_pen_def),
    sizeof(struct oem_brush_def),
    0
};

static const robj_call_t robj_call[GDIOBJ_FONT] = {
    my_realize_pen,
    my_realize_brush,
    my_realize_font
};

DWORD WINAPI my_RealizeObject(LPVOID lpDestDev, WORD wStyle, LPVOID lpInObj, LPVOID lpOutObj, LPTEXTXFORM lpTextXForm) {
    if (wStyle == 0/*deleting*/)
        return 1; /* nothing to do */
    if (wStyle > GDIOBJ_FONT)
        return 0;

    if (lpOutObj == NULL)
        return robj_sizes[wStyle-1u];
    else
        return robj_call[wStyle-1u](lpDestDev,lpInObj,lpOutObj,lpTextXForm);

    return 0;
}

void WINAPI my_StrBlt(void) {
    __asm int 3
    __asm mov ax,11
}

void WINAPI my_ScanLR(void) {
    __asm int 3
    __asm mov ax,12
}

void WINAPI my_DeviceMode(void) {
    __asm int 3
    __asm mov ax,13
}

BOOL WINAPI my_BitBlt(LPPDEVICE lpDestDev,WORD wDestX,WORD wDestY,LPPDEVICE lpSrcDev,WORD wSrcX,WORD wSrcY,WORD wXext,WORD wYext,long Rop3,/*LPBRUSH*/LPSTR lpPBrush,LPDRAWMODE lpDrawMode) {
    return TRUE;
}

DWORD WINAPI my_ExtTextOut(LPPDEVICE lpDestDev,WORD wDestXOrg,WORD wDestYOrg,LPRECT lpClipRect,LPSTR lpString,int wCount,LPFONTINFO lpFontInfo,LPDRAWMODE lpDrawMode,LPTEXTXFORM lpTextXForm,LPSHORT lpCharWidths,LPRECT lpOpaqueRect,WORD wOptions) {
    return 0;
}

void WINAPI my_GetCharWidth(void) {
    __asm int 3
    __asm mov ax,15
}

void WINAPI my_DeviceBitmap(void) {
    __asm int 3
    __asm mov ax,16
}

void WINAPI my_FastBorder(void) {
    __asm int 3
    __asm mov ax,17
}

void WINAPI my_SetAttribute(void) {
    __asm int 3
    __asm mov ax,18
}

void WINAPI my_DeviceBitmapBits(LPPDEVICE lpBitmap,WORD fGet,WORD iStart,WORD cScans,LPSTR lpDIBits,LPBITMAPINFO lpBitmapInfo,LPDRAWMODE lpDrawMode,LPINT lpTranslate) {
}

void WINAPI my_CreateBitmap(void) {
    __asm int 3
    __asm mov ax,20
}

void WINAPI my_DIBScreenBlt(void) {
    __asm int 3
    __asm mov ax,21
}

void WINAPI my_do_polylines(void) {
    __asm int 3
    __asm mov ax,90
}

void WINAPI my_do_scanlines(void) {
    __asm int 3
    __asm mov ax,91
}

#pragma pack(push,1)
typedef struct tagCURSORINFO {
    short   dpXRate;
    short   dpYRate;
} CURSORINFO;
typedef CURSORINFO FAR *LPCURSORINFO;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct tagCURSORSHAPE {
    short csHotX;
    short csHotY;
    short csWidth;
    short csHeight;
    short csWidthBytes;
    short csColor;
    char  csBits[1];
} CURSORSHAPE;
typedef CURSORSHAPE FAR *LPCURSORSHAPE;
#pragma pack(pop)

WORD WINAPI my_Inquire(LPCURSORINFO lpCursorInfo) {
    lpCursorInfo->dpXRate = 1;
    lpCursorInfo->dpYRate = 2;
    return sizeof(CURSORINFO);
}

void WINAPI my_SetCursor(LPCURSORSHAPE lpCursorShape) {
}

void WINAPI my_MoveCursor(WORD wAbsX,WORD wAbsY) { /* caution: May be called from an interrupt */
}

void WINAPI my_CheckCursor(void) { /* caution: May be called from an interrupt */
}

void WINAPI my_UserRepaintDisable(BOOL bRepaintDisable) { /* not documented in the DDK */
}

