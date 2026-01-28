
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <conio.h>

#include <hw/cpu/cpu.h>
#include <hw/dosboxid/iglib.h>

#pragma pack(push,1)
#pragma pack(pop)

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

    if (!vga_memsize) return 0;

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
    __asm int 3
    __asm mov ax,5
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
    __asm int 3
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

