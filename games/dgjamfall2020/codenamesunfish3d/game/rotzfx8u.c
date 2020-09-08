
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"

/* "Second Reality" style rotozoomer because Woooooooooooo */
void rotozoomer_fast_effect(unsigned int w,unsigned int h,__segment imgseg,const uint32_t rt) {
    const __segment vseg = FP_SEG(vga_state.vga_graphics_ram);

    // scale, to zoom in and out
    const long scale = ((long)sin2048fps16_lookup(rt * 5ul) >> 1l) + 0xA000l;
    // column-step. multiplied 4x because we're rendering only every 4 pixels for smoothness.
    const uint16_t sx1 = (uint16_t)(((((long)cos2048fps16_lookup(rt * 10ul) *  0x400l) >> 15l) * scale) >> 15l);
    const uint16_t sy1 = (uint16_t)(((((long)sin2048fps16_lookup(rt * 10ul) * -0x400l) >> 15l) * scale) >> 15l);
    // row-step. multiplied by 1.2 (240/200) to compensate for 320x200 non-square pixels. (1.2 * 0x100) = 0x133
    const uint16_t sx2 = (uint16_t)(((((long)cos2048fps16_lookup(rt * 10ul) *  0x133l) >> 15l) * scale) >> 15l);
    const uint16_t sy2 = (uint16_t)(((((long)sin2048fps16_lookup(rt * 10ul) * -0x133l) >> 15l) * scale) >> 15l);

    unsigned cvo = FP_OFF(vga_state.vga_graphics_ram);
    uint16_t fcx,fcy;

// make sure rotozoomer is centered on screen
    fcx = 0 - ((w / 2u / 4u) * sx1) - ((h / 2u) *  sy2);
    fcy = 0 - ((w / 2u / 4u) * sy1) - ((h / 2u) * -sx2);

// NTS: Because of VGA unchained mode X, this renders the effect in vertical columns rather than horizontal rows
    while (w >= 4) {
        vga_write_sequencer(0x02/*map mask*/,0x0F);

        // WARNING: This loop temporarily modifies DS. While DS is altered do not access anything by name in the data segment.
        //          However variables declared locally in this function are OK because they are allocated from the stack, and
        //          are referenced using [bp] which uses the stack (SS) register.
        __asm {
            mov     cx,h
            mov     es,vseg
            mov     di,cvo
            inc     cvo
            push    ds
            mov     ds,imgseg
            mov     dx,fcx          ; DX = fractional X coord
            mov     si,fcy          ; SI = fractional Y coord

crloop:
            ; BX = (SI & 0xFF00) + (DX >> 8)
            ;   but DX/BX are the general registers with hi/lo access so
            ; BX = (SI & 0xFF00) + DH
            ; by the way on anything above a 386 that sort of optimization stuff doesn't help performance.
            ; later (Pentium Pro) processors don't like it when you alternate between DH/DL and DX because
            ; of the way the processor works internally regarding out of order execution and register renaming.
            mov     bx,si
            mov     bl,dh
            mov     al,[bx]
            stosb
            add     di,79           ; stosb / add di,79  is equivalent to  mov es:[di],al / add di,80

            add     dx,sy2
            sub     si,sx2

            loop    crloop
            pop     ds
        }

        fcx += sx1;
        fcy += sy1;
        w -= 4;
    }
}

