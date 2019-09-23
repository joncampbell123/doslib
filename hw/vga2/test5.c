
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

void cls(void) {
    VGA2_ALPHA_PTR p = vga2_alpha_ptr();
    unsigned int c = vga2_alpha_base.width * vga2_alpha_base.height;

    while (c > 0) {
#if defined(TARGET_PC98)
#else
        *p++ = VGA2_ALPHA_ATTR_FGBGCOLOR(7,0) | 0x20;
#endif
        c--;
    }
}

#if !defined(TARGET_PC98)
/* EGA/VGA only! */
void vga2_set_alpha_stride_egavga(unsigned int w) {
    const uint16_t port = vga2_bios_crtc_io();
    outp(port+0,0x13);
    outp(port+1,w>>1u); /* offset register, even values only */
    vga2_alpha_base.width = w & ~1u; /* even values only */
    *vga2_bda_w(0x4A) = w & ~1u; /* update BIOS too */
}

/* EGA only (VGA requires consideration of CRTC write protect) */
void vga2_set_alpha_display_width_ega(unsigned int w) {
    const uint16_t port = vga2_bios_crtc_io();
    w &= ~1u; /* EGA/VGA can only allow even number of columns. Horizontal display end CAN allow it but offset register cannot */
    outp(port+0,0x01);
    outp(port+1,w-1u); /* horizontal display end */
    outp(port+0,0x02);
    outp(port+1,w+1u); /* start horizontal blanking. between active and blanking exists the overscan border */
    /* affects display width, not stride */
}

/* VGA only (write protect in the way).
 * For simplicity sake (for now) calls EGA version after unlocking registers */
void vga2_set_alpha_display_width_vga(unsigned int w) {
    const uint16_t port = vga2_bios_crtc_io();
    uint8_t p;
    outp(port+0,0x11); /* vertical retrace end / bandwith / protect */
    p = inp(port+1);
    outp(port+1,p & 0x7Fu); /* unlock */
    w &= ~1u; /* EGA/VGA can only allow even number of columns. Horizontal display end CAN allow it but offset register cannot */
    outp(port+0,0x01);
    outp(port+1,w-1u); /* horizontal display end */
    outp(port+0,0x02);
    outp(port+1,w+1u); /* start horizontal blanking. between active and blanking exists the overscan border */
    /* affects display width, not stride */
    outp(port+0,0x11); /* vertical retrace end / bandwith / protect */
    outp(port+1,p); /* restore */
}

/* CGA/MDA/PCjr/Tandy. Stride (char per row) is determined by display columns */
/* TODO: It is *UNKNOWN* whether this works on MCGA since MCGA has hardware support
 *       for figuring out horizontal timing automatically and ignoring these registers,
 *       which is on by default apparently. */
void vga2_set_alpha_display_width_cga(unsigned int w) {
    const uint16_t port = vga2_bios_crtc_io();
    outp(port+0,0x01);
    outp(port+1,w); /* horizontal display end */
    vga2_alpha_base.width = w;
    *vga2_bda_w(0x4A) = w; /* update BIOS too */
}

#endif

unsigned int do_test(unsigned int w) {
    unsigned int r = 1;
    int c;

#if defined(TARGET_PC98)
#else
    if (vga2_flags & (VGA2_FLAG_EGA|VGA2_FLAG_VGA)) {
        vga2_set_alpha_stride_egavga(w);
        if (vga2_flags & VGA2_FLAG_VGA)
            vga2_set_alpha_display_width_vga(w);
        else
            vga2_set_alpha_display_width_ega(w);
    }
    else {
        vga2_set_alpha_display_width_cga(w);
    }
#endif

    cls();
#if defined(TARGET_PC98)
#else
    vga2_set_int10_cursor_pos(2,0);
#endif
    printf("Hello w=%u mode %u x %u\n",w,vga2_alpha_base.width,vga2_alpha_base.height);
    printf("Hello 2\n");
    printf("Hello 3\n");
    do {
        c = getch();
        if (c == 13) break;
        if (c == 27) {
            r = 0;
            break;
        }
    } while (1);

    return r;
}

int main(int argc,char **argv) {
    /* base classicifcation */
    probe_vga2();
    vga2_update_alpha_modeinfo();
    if (!vga2_alpha_ptr_valid()) {
        printf("Unable to determine text buffer\n");
        return 1;
    }

    if (!do_test(80)) return 0;
    if (!do_test(82)) return 0;
    if (!do_test(85)) return 0;
    if (!do_test(90)) return 0;
    if (!do_test(79)) return 0;
    if (!do_test(78)) return 0;
    if (!do_test(70)) return 0;
    if (!do_test(60)) return 0;
    if (!do_test(50)) return 0;
    if (!do_test(40)) return 0;
    if (!do_test(30)) return 0;
    if (!do_test(20)) return 0;
    if (!do_test(84)) return 0;

    return 0;
}

