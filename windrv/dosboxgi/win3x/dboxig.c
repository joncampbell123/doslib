
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <conio.h>

#include <hw/cpu/cpu.h>
#include <hw/dosboxid/iglib.h>

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
    if (!(r & DOSBOX_ID_REG_VGA1G_CAPS_ENABLED)) {
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

    __asm int 3
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

void WINAPI my_BitBlt(void) {
    __asm int 3
    __asm mov ax,1
}

void WINAPI my_ColorInfo(void) {
    __asm int 3
    __asm mov ax,2
}

void WINAPI my_Control(void) {
    __asm int 3
    __asm mov ax,3
}

void WINAPI my_Disable(LPSTR/*FIXME*/ lpDestDev) {
    __asm int 3
    __asm mov ax,4
}

WORD WINAPI my_Enable(LPVOID *lpDevInfo, WORD wStyle, LPSTR lpDestDevType, LPSTR lpOutputFile, LPVOID lpData) {
    (void)lpDestDevType;
    (void)lpOutputFile;
    (void)lpData;

    if (wStyle & InquireInfo) {
        _copymem((LPGDIINFO*)lpDevInfo,&GDIInfo,sizeof(GDIINFO));
        return sizeof(GDIINFO);
    }

    return 0;
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

void WINAPI my_RealizeObject(void) {
    __asm int 3
    __asm mov ax,10
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

void WINAPI my_ExtTextOut(void) {
    __asm int 3
    __asm mov ax,14
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

void WINAPI my_DeviceBitmapBits(void) {
    __asm int 3
    __asm mov ax,19
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

WORD WINAPI my_Inquire(LPCURSORINFO lpCursorInfo) {
    lpCursorInfo->dpXRate = 1;
    lpCursorInfo->dpYRate = 2;
    return sizeof(CURSORINFO);
}

void WINAPI my_SetCursor(void) {
    __asm int 3
    __asm mov ax,102
}

void WINAPI my_MoveCursor(void) {
    __asm int 3
    __asm mov ax,103
}

void WINAPI my_CheckCursor(void) {
    __asm int 3
    __asm mov ax,104
}

void WINAPI my_UserRepaintDisable(void) {
    __asm int 3
    __asm mov ax,500
}

