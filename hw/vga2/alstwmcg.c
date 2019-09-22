
#include <i86.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#ifndef TARGET_PC98
// for CGA/MDA/PCjr/Tandy
// Modify the horizonal display width.
// Hardware does not support stride != width, display width determines char per row.
// do NOT use with MCGA, the value programmed must be different.
void vga2_set_alpha_width_cga(const unsigned int w,const unsigned int str) {
    /* update the CRTC */
    {
        /* NOTED: This BDA variable exists in the 1986 published IBM PC BIOS listing */
        /* NOTED: This BDA variable exists in the 1983 published IBM PC/XT BIOS listing */
        /* that means unlike the "rows" variable no consideration needs to be made of BIOSes that don't use it */
        const uint16_t port = vga2_bios_crtc_io(); /* 0x3B4 or 3D4 */
        outp(port+0u,0x01u); /* Horizontal Displayed Register */
        outp(port+1u,(uint8_t)w); /* in character cells */
    }
    /* update the BIOS data area */
    *vga2_bda_w(0x4A) = (uint16_t)w;
    /* update internal var */
    vga2_alpha_base.width = (uint8_t)w;
}
#endif

