
#include <i86.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#if defined(TARGET_PC98)
static char tmp[256];

void my_pc98_write_str(unsigned int r,unsigned int c,const char *s) {
    VGA2_ALPHA_PTR p = vga2_alpharowcol_ptr(r,c);
    char cc;

    /* this won't do Shift JIS decoding, sorry */
    while ((cc = *s++) != 0) {
        p[0] = (unsigned char)cc;
        p[VGA2_PC98_ATTR_OFFSC] = VGA2_ALPHA_ATTR_NOTSECRET | VGA2_ALPHA_ATTR_COLOR(7);
        p++;
    }
}
#endif

void cls(void) {
    VGA2_ALPHA_PTR p = vga2_alpha_ptr();
    unsigned int c = vga2_alpha_base.width * vga2_alpha_base.height;

    while (c > 0) {
#if defined(TARGET_PC98)
        p[0] = 0x20;
        p[VGA2_PC98_ATTR_OFFSC] = VGA2_ALPHA_ATTR_NOTSECRET | VGA2_ALPHA_ATTR_COLOR(7);
        p++;
#else
        *p++ = VGA2_ALPHA_ATTR_FGBGCOLOR(7,0) | 0x20;
#endif
        c--;
    }
}

#if defined(TARGET_PC98)

static inline void vga2_pc98_text_gdc_write_command(const unsigned char cmd) {
    while (inp(0x60) & 0x02); /* while FIFO full */
    outp(0x60+2,cmd); // write command
}

static inline void vga2_pc98_text_gdc_write_data(const unsigned char d) {
    while (inp(0x60) & 0x02); /* while FIFO full */
    outp(0x60,d); // write data
}

/* PC-98 only */
void vga2_set_alpha_stride_pc98(unsigned int w) {
    vga2_pc98_text_gdc_write_command(0x47/*Pitch specification*/);
    vga2_pc98_text_gdc_write_data(w);
    vga2_alpha_base.width = w; /* even values only */
}
#else
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

/* VGA only (write protect in the way) */
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

/* MCGA only (write protect in the way). */
/* NTS: This code has been tested and confirmed to work on MCGA hardware */
void vga2_set_alpha_display_width_mcga(unsigned int w) {
    const uint16_t port = 0x3D4; /* MCGA is always color, there's no monochrome MCGA that I'm aware of at 3B4h */
    unsigned int fw;
    uint8_t p,q;

    /* MCGA is weird.
     * 80x25 mode has horizontal timings programmed as if 40x25.
     * In fact all modes are programming as if 40x25.
     * This is probably why the PS/2 BIOS sets write protect for 80x25 mode 2 and 3.
     * The unfortunate result is that the final value of w depends on whether a 40x25
     * or 80x25 mode is in effect, which we have to figure out.
     *
     * One way to tell it seems is to look at the 80-column enable bit in 3D8h.
     * Fortunately it seems, 3D8h is readable on MCGA, unlike the original CGA hardware.
     *
     * However that won't detect the 640x480 2-color mode, but there's a bit in the MCGA
     * mode set aside specifically to enable that mode, which we can see. */
    outp(port+0,0x10); /* mcga mode / protect */
    p = inp(port+1);
    outp(port+1,p & 0x7Fu); /* unlock */

    q = inp(0x3D8); /* CGA compatible mode control */

    if ((p & 2)/*enable for 640x480 2-color*/ || (q & 1)/*enable for CGA 80-column*/) {
        w &= ~1u;
        fw = w >> 1u;
    }
    else {
        fw = w;
    }

    outp(port+0,0x01);
    outp(port+1,fw-1); /* horizontal display end */
    outp(port+0,0x10); /* mcga mode / protect */
    outp(port+1,p); /* restore */
    vga2_alpha_base.width = w;
    *vga2_bda_w(0x4A) = w; /* update BIOS too */
}

/* CGA/MDA/PCjr/Tandy. Stride (char per row) is determined by display columns */
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
    vga2_set_alpha_stride_pc98(w);
    /* changing active display width is way more involved.
     * it involves re-sending the mode to the GDC.
     * if it turns out some table or pointer is available to read those
     * values that we can read and copy, then we can use that here in
     * the future. */
#else
    if (vga2_flags & (VGA2_FLAG_EGA|VGA2_FLAG_VGA)) {
        vga2_set_alpha_stride_egavga(w);
        if (vga2_flags & VGA2_FLAG_VGA)
            vga2_set_alpha_display_width_vga(w);
        else
            vga2_set_alpha_display_width_ega(w);
    }
    else if (vga2_flags & VGA2_FLAG_MCGA) {
        vga2_set_alpha_display_width_mcga(w);
    }
    else {
        vga2_set_alpha_display_width_cga(w);
    }
#endif

    cls();
#if defined(TARGET_PC98)
    /* Unlike IBM PC, I do not think PC-98 and the MS-DOS console driver support
     * anything other than 80 columns, so for the effect to work we have to draw
     * our own text.
     *
     * It also means that unlike IBM PC we have to put the console back to 80
     * columns when we're done.
     *
     * Declaring a char[] array on stack is not wise in 16-bit real mode because
     * the stack is typically very small, use a global char tmp[] instead. */
    {
        sprintf(tmp,"Hello w=%u mode %u x %u",w,vga2_alpha_base.width,vga2_alpha_base.height);
        my_pc98_write_str(2/*row*/,0/*col*/,tmp);
        sprintf(tmp,"Hello 2");
        my_pc98_write_str(3/*row*/,0/*col*/,tmp);
        sprintf(tmp,"Hello 3");
        my_pc98_write_str(4/*row*/,0/*col*/,tmp);
    }
#else
    vga2_set_int10_cursor_pos(2,0);
    printf("Hello w=%u mode %u x %u\n",w,vga2_alpha_base.width,vga2_alpha_base.height);
    printf("Hello 2\n");
    printf("Hello 3\n");
#endif

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
    (void)argc;
    (void)argv;

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
#if defined(TARGET_PC98)
    if (!do_test(80)) return 0;
#endif

    return 0;
}

