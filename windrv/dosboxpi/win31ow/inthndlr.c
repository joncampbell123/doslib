
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
unsigned char __based( __segname("_NDDATA") ) cur_status = 0xFF;

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

void interrupt int15_handler() {
    /* TODO: YES!!! This handler works! */
}

