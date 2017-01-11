
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>

#include "iglib.h"

#include "mouse.h"

const void far * __based( __segname("_NDDATA") ) AssignedEventProc = NULL;
unsigned short __based( __segname("_NDDATA") ) prev_x = 0xFFFFU;
unsigned short __based( __segname("_NDDATA") ) prev_y = 0xFFFFU;
unsigned char __based( __segname("_NDDATA") ) prev_status = 0;

/* mouse button change lookup (PPBB) P=previous button state B=current button state */
/* this is const (readonly) data, so stick it in with the code segment where Win16
 * protected mode prevents writing to code segments anyway */
const unsigned char __based( __segname("_CODE") ) mousebutton_lookup[4*4] = {
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
 *   [saved AX]                     <- our callback                        <- SS:SP right before near call to this function
 *
 *   __cdecl passes "right to left", meaning that arguments are pushed onto the stack in that order.
 *
 *   FIXME: How do we convince Watcom to load DS register with segment value of _NDDATA?
 *          Looking at the disassembly output of this function, Watcom seems keen on ignoring DS
 *          and loading ES instead to refer to our local variables in the _NDDATA segment.
 *          __loadds is useless here, because it will only load DGROUP instead.
 *          es: prefixes on memory references are a bit wasteful.
 *   */
static void __cdecl near int15_handler_C(const unsigned short _ax,const unsigned short _ds,const unsigned short _es,const unsigned short retn,const unsigned short retf,const unsigned short word4,const unsigned short ydata,const unsigned short xdata,const unsigned short status) {
    unsigned short win_status = SF_ABSOLUTE;
    unsigned short pos_x,pos_y;

    _cli();

    {
        unsigned long r;

        dosbox_id_write_regsel(DOSBOX_ID_REG_USER_MOUSE_CURSOR_NORMALIZED);
        r = dosbox_id_read_data();
        pos_x = (unsigned short)(r & 0xFFFFUL);
        pos_y = (unsigned short)(r >> 16UL);

        if (((pos_x - prev_x) | (pos_y - prev_y)) != 0) /* if X or Y changed, there's movement */
            win_status |= SF_MOVEMENT;

        prev_x = pos_x;
        prev_y = pos_y;
    }

    {
        /* take button status from status given by BIOS and update win_status with it.
         * use lookup table: PPBB (P=prev button state B=current button state) */
        unsigned char lookup;

        lookup = (prev_status << 2) | (status & 3);
        prev_status = status & 3;
        win_status |= mousebutton_lookup[lookup];
    }

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
            call        dword ptr [es:AssignedEventProc]
        }
    }
}

void __declspec(naked) far int15_handler() {
    __asm {
        push    ax      ; because __cdecl won't preserve AX
        push    ds      ; <- just in case
        push    es      ; and Watcom won't bother saving ES
        call    int15_handler_C
        pop     es
        pop     ds
        pop     ax
        retf
    }
}

