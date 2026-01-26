
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

DWORD vga_memsize = 0;
DWORD vga_lfb = 0;

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
    const char *scan;
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

int init_dosbox_ig(void) {
    uint32_t r;

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

    DEBUG_OUTF("VGA lfb=0x%lx memsize=0x%lx(%lu)\n",(unsigned long)vga_lfb,(unsigned long)vga_memsize,(unsigned long)vga_memsize);
    if (!vga_memsize) return 0;

    return 1;
}

WORD MiniLibMain(void) {
    if (!probe_dosbox_id())
        return 0;
    if (dos_version() < 3)
        return 0;

    __asm int 3
    DEBUG_OUT("DOSBox IG display driver init\n");

    if (!init_dosbox_ig())
        return 0;

    return 1;
}

