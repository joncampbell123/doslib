
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>

#include "iglib.h"

#include "mouse.h"

const void far *AssignedEventProc = NULL;

static unsigned short prev_x = 0xFFFFU;
static unsigned short prev_y = 0xFFFFU;
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
    unsigned short win_status = SF_ABSOLUTE;
    unsigned short pos_x,pos_y;

    _cli();
    dosbox_id_push_state(); /* user-space may very well in the middle of working with the DOSBOX IG, save state */

    {
        unsigned long r;

        /* read user mouse status.
         * if the user's cursor is locked (captured), then the PS/2 mouse data is relative motion,
         * and we would be better off sending relative motion like a PS/2 mouse. absolute motion
         * is for when mouse capture is NOT active */
        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_STATUS);
        r = dosbox_id_read_data();

        if (r & DOSBOX_ID_REG_USER_MOUSE_STATUS_CAPTURE) {
            /* DOSBox-X has captured the user's mouse. Send relative motion. */
            win_status &= ~SF_ABSOLUTE;

            pos_x = (unsigned short)  ((int8_t)xdata); /* sign-extend 8-bit motion */
            pos_y = (unsigned short) -((int8_t)ydata); /* sign-extend 8-bit motion, flip Y direction */

            if ((pos_x | pos_y) != 0)
                win_status |= SF_MOVEMENT;
        }
        else {
            dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
            r = dosbox_id_read_data();
            pos_x = (unsigned short)(r & 0xFFFFUL);
            pos_y = (unsigned short)(r >> 16UL);

            if (((pos_x - prev_x) | (pos_y - prev_y)) != 0) /* if X or Y changed, there's movement */
                win_status |= SF_MOVEMENT;

            prev_x = pos_x;
            prev_y = pos_y;
        }
    }

    {
        /* take button status from status given by BIOS and update win_status with it.
         * use lookup table: PPBB (P=prev button state B=current button state) */
        unsigned char lookup;

        lookup = (prev_status << 2) | (status & 3);
        prev_status = status & 3;
        win_status |= mousebutton_lookup[lookup];
    }

    dosbox_id_pop_state(); /* restore state, so user-space's work continues uninterrupted */
    _sti();

    if ((win_status & ~(SF_ABSOLUTE)) != 0) {
        /* call Windows */
        __asm {
            mov         ax,win_status
            mov         bx,pos_x
            mov         cx,pos_y
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

