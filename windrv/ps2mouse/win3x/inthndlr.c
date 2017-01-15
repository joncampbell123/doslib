
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>

#include "mouse.h"

const void far *AssignedEventProc = NULL;

static unsigned char prev_status = 0;

/* mouse button change lookup (PPBB) P=previous button state B=current button state */
/* this is const (readonly) data, so stick it in with the code segment where Win16
 * protected mode prevents writing to code segments anyway */
static const unsigned char __based( __segname("_CODE") ) mousebutton_lookup[4*4] = {
// P=00 B=xx
    0,
    SF_B1_DOWN,
    SF_B2_DOWN,
    SF_B1_DOWN + SF_B2_DOWN,
// P=01 B=xx
    SF_B1_UP,
    0,
    SF_B1_UP + SF_B2_DOWN,
    SF_B2_DOWN,
// P=10 B=xx
    SF_B2_UP,
    SF_B1_DOWN+SF_B2_UP,
    0,
    SF_B1_DOWN,
// P=11 B=xx
    SF_B2_UP+SF_B1_UP,
    SF_B2_UP,
    SF_B1_UP,
    0
};

/* __cdecl pushes right-to-left, so given [bp+ofs], [bp+ofs+0] would be _ax, [bp+ofs+6] would be status, etc.
 * do you see how I am using the function prototype to access the stack here?
 *
 *   [status]                       <- BIOS provided info
 *   [xdata]
 *   [ydata]
 *   [word4]
 *   [return address segment]       <- BIOS far call to our callback
 *   [return address offset]
 *   [saved AX]                     <- our callback
 *   [saved BX]
 *   [saved CX]
 *   [saved DX]
 *   [saved ES]                                                         <- SS:SP right before near call to this function
 *
 *   __cdecl passes "right to left", meaning that arguments are pushed onto the stack in that order.
 */
static void __cdecl near __loadds int15_handler_C(const unsigned short _es,const unsigned short _dx,const unsigned short _cx,const unsigned short _bx,const unsigned short _ax,const unsigned short retn,const unsigned short retf,const unsigned short word4,const unsigned short ydata,const unsigned short xdata,const unsigned short status) {
    int16_t rel_x =   (int16_t)((int8_t)xdata);
    int16_t rel_y = -((int16_t)((int8_t)ydata)); /* NTS: need to flip Y direction */
    unsigned short win_status = 0;

    _cli();

    if ((rel_x|rel_y) != 0)
        win_status = SF_MOVEMENT;

    {
        /* take button status from status given by BIOS and update win_status with it.
         * use lookup table: PPBB (P=prev button state B=current button state) */
        unsigned char lookup;

        lookup = (prev_status << 2) | (status & 3);
        prev_status = status & 3;
        win_status |= mousebutton_lookup[lookup];
    }

    _sti();

    if (win_status != 0) {
        /* call Windows */
        __asm {
            mov         ax,win_status
            mov         bx,rel_x
            mov         cx,rel_y
            mov         dx,2            ; number of buttons
            xor         si,si
            xor         di,di
            call        dword ptr [AssignedEventProc]
        }
    }
}

void __declspec(naked) far int15_handler() {
    __asm {
        push    ax      ; because __cdecl won't preserve AX
        push    bx      ; or BX
        push    cx      ; or CX
        push    dx      ; or DX
        push    es      ; and Watcom won't bother saving ES
        call    int15_handler_C
        pop     es
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        retf
    }
}

