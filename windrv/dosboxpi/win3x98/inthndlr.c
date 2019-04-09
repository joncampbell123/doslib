
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>
#include <hw/dosboxid/iglib.h>

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

/* WARNING: This will be called 60 times per second, because that is how interrupts from the PC-98 bus mouse work */
static void __cdecl near __loadds interrupt_handler_C(void) {
    unsigned short win_status = SF_ABSOLUTE;
    unsigned short pos_x,pos_y;
    unsigned char status = 0;

    _cli();
    dosbox_id_push_state(); /* user-space may very well in the middle of working with the DOSBOX IG, save state */

    // read button state.
    // NTS: Convert 3-button info to 2-button L:R state
    {
        unsigned char t = ~inp(0x7FD9);
        if (t & 0x80) status |= 1;// left button
        if (t & 0x20) status |= 2;// right button
    }

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

            // read bus mouse motion
            {
                unsigned char t;

                // ----------------- X pos ----------------
                outp(0x7FDD,(0 << 5) | 0x80); // X-pos low nibble, latch position
                t  =  inp(0x7FD9) & 0xF;

                outp(0x7FDD,(1 << 5) | 0x80); // X-pos high nibble, latch position
                t |= (inp(0x7FD9) & 0xF) << 4u;

                pos_x = (unsigned int)t;
                if (t & 0x80) pos_x |= 0xFF80u;

                // ----------------- Y pos ----------------
                outp(0x7FDD,(2 << 5) | 0x80); // Y-pos low nibble, latch position
                t  =  inp(0x7FD9) & 0xF;

                outp(0x7FDD,(3 << 5) | 0x80); // Y-pos high nibble, latch position
                t |= (inp(0x7FD9) & 0xF) << 4u;

                pos_y = (unsigned int)t;
                if (t & 0x80) pos_y |= 0xFF80u;

                // ----------------- DONE -----------------
                outp(0x7FDD,0x00);          // clear latch. make sure interrupt is not masked
            }

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
        if (AssignedEventProc != NULL) {
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
}

void __declspec(naked) far interrupt_handler() {
    __asm {
        push    ax      ; because __cdecl won't preserve AX
        push    bx      ; or BX
        push    cx      ; or CX
        push    dx      ; or DX
        push    es      ; and Watcom won't bother saving ES
        call    interrupt_handler_C
        mov     al,0x20
        out     0x08,al ; ack slave
        out     0x00,al ; ack master
        pop     es
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        iretf
    }
}

