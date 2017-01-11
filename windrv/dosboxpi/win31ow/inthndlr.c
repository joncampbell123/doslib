
#include <windows.h>
#include <stdlib.h>
#include <stdint.h>
#include <conio.h>

#include <hw/cpu/cpu.h>

#include "iglib.h"

unsigned short __based( __segname("_NDDATA") ) prev_x = 0xFFFFU;
unsigned short __based( __segname("_NDDATA") ) prev_y = 0xFFFFU;
unsigned char __based( __segname("_NDDATA") ) prev_status = 0;
unsigned char __based( __segname("_NDDATA") ) cur_status = 0xFF;

void hello() {
    __asm int 3
}

